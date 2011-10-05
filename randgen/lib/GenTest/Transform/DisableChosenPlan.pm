# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
# USA

package GenTest::Transform::DisableChosenPlan;

require Exporter;
@ISA = qw(GenTest GenTest::Transform);

use strict;
use lib 'lib';
use GenTest;
use GenTest::Transform;
use GenTest::Constants;
use Data::Dumper;

#
# This Transform runs EXPLAIN on the query, determines which (subquery) optimizations were used
# and disables them so that the query can be rerun with a "second-best" plan. This way the best and
# the "second best" plans are checked against one another.
#
# This has the following benefits:
# 1. The query plan that is being validated is the one actually chosen by the optimizer, so that one can
# run a comprehensive subquery test without having to manually fiddle with @@optimizer_switch
# 2. The plan that is used for validation is hopefully also fast enough, as compared to using unindexed nested loop
# joins with re-execution of the enitre subquery for each loop.
#

my @explain2switch = (
	[ 'sort_intersect'	=> "optimizer_switch='index_merge_sort_intersection=off'"],
	[ 'intersect'		=> "optimizer_switch='index_merge_intersection=off'"],
	[ 'firstmatch'		=> "optimizer_switch='firstmatch=off'" ],
	[ '<expr_cache>'	=> "optimizer_switch='subquery_cache=off'" ],
	[ 'materializ'		=> "optimizer_switch='materialization=off'" ],	# hypothetical
	[ 'semijoin'		=> "optimizer_switch='semijoin=off'" ],
	[ 'loosescan'		=> "optimizer_switch='loosescan=off'" ],
	[ '<exists>'		=> "optimizer_switch='in_to_exists=off'" ],
	[ qr{BNLH|BKAH}		=> "optimizer_switch='join_cache_hashed=off'" ],	
	[ 'BKA'			=> "optimizer_switch='join_cache_bka=off'" ],
	[ 'incremental'		=> "optimizer_switch='join_cache_incremental=off'" ],
	[ 'join buffer'		=> "join_cache_level=0" ],
	[ 'join buffer'		=> "optimizer_join_cache_level=0" ],
	[ 'mrr'			=> "optimizer_use_mrr='disable'" ],
	[ 'index condition'	=> "optimizer_switch='index_condition_pushdown=off'" ]
);

my $available_switches;

sub transform {
	my ($class, $original_query, $executor) = @_;

	if (not defined $available_switches) {
		my $optimizer_switches = $executor->dbh()->selectrow_array('SELECT @@optimizer_switch');
		my @optimizer_switches = split(',', $optimizer_switches);
		foreach my $optimizer_switch (@optimizer_switches) {
			my ($switch_name, $switch_value) = split('=', $optimizer_switch);
			$available_switches->{"optimizer_switch='$switch_name=off'"}++;
		}

		if ($executor->dbh()->selectrow_array('SELECT @@optimizer_use_mrr')) {
			$available_switches->{"optimizer_use_mrr='disable'"}++;
		}

		if ($executor->dbh()->selectrow_array('SELECT @@join_cache_level')) {
			$available_switches->{"join_cache_level=0"}++;
		}

		if ($executor->dbh()->selectrow_array('SELECT @@optimizer_join_cache_level')) {
			$available_switches->{"optimizer_join_cache_level=0"}++;
		}
	}

	return STATUS_WONT_HANDLE if $original_query !~ m{^\s*SELECT}sio;

	my $original_explain = $executor->execute("EXPLAIN EXTENDED $original_query");

	if ($original_explain->status() == STATUS_SERVER_CRASHED) {
		return STATUS_SERVER_CRASHED;
	} elsif ($original_explain->status() ne STATUS_OK) {
		return STATUS_ENVIRONMENT_FAILURE;
	}

	my $original_explain_string = Dumper($original_explain->data())."\n".Dumper($original_explain->warnings());

	my @transformed_queries;
	foreach my $explain2switch (@explain2switch) {
		my ($explain_fragment, $optimizer_switch) = ($explain2switch->[0], $explain2switch->[1]);
		next if not exists $available_switches->{$optimizer_switch};
		if ($original_explain_string =~ m{$explain_fragment}si) {
			my ($switch_name) = $optimizer_switch =~ m{^(.*?)=}sgio;
			push @transformed_queries, (
				'SET @switch_saved = @@'.$switch_name.';',
				"SET SESSION $optimizer_switch;",
				"$original_query /* TRANSFORM_OUTCOME_UNORDERED_MATCH */ ;",
				'SET SESSION '.$switch_name.'=@switch_saved'
			);
		}
	}

	if ($#transformed_queries > -1) {
		return \@transformed_queries;
	} else {
		return STATUS_WONT_HANDLE;
	}
}

1;
