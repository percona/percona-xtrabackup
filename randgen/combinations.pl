# Copyright (c) 2008, 2011 Oracle and/or its affiliates. All rights reserved.
# Use is subject to license terms.
#
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

use strict;
use lib 'lib';
use lib "$ENV{RQG_HOME}/lib";
use List::Util 'shuffle';
use GenTest;
use GenTest::Random;
use GenTest::Constants;
use Getopt::Long;
use Data::Dumper;

my ($config_file, $basedir, $vardir, $trials, $duration, $grammar, $gendata, 
    $seed, $testname, $xml_output, $report_xml_tt, $report_xml_tt_type,
    $report_xml_tt_dest, $force, $no_mask, $exhaustive, $debug);

my $combinations;
my %results;
my @commands;
my $max_result = 0;

my $opt_result = GetOptions(
	'config=s' => \$config_file,
	'basedir=s' => \$basedir,
	'vardir=s' => \$vardir,
	'trials=i' => \$trials,
	'duration=i' => \$duration,
	'seed=s' => \$seed,
	'force' => \$force,
	'no-mask' => \$no_mask,
	'grammar=s' => \$grammar,
	'gendata=s' => \$gendata,
	'testname=s' => \$testname,
	'xml-output=s' => \$xml_output,
	'report-xml-tt' => \$report_xml_tt,
	'report-xml-tt-type=s' => \$report_xml_tt_type,
	'report-xml-tt-dest=s' => \$report_xml_tt_dest,
    'run-all-combinations-once' => \$exhaustive,
    'debug' => \$debug
);


my $prng = GenTest::Random->new(
	seed => $seed eq 'time' ? time() : $seed
);

open(CONF, $config_file) or die "unable to open config file '$config_file': $!";
read(CONF, my $config_text, -s $config_file);
eval ($config_text);
die "Unable to load $config_file: $@" if $@;

mkdir($vardir);
system("bzr version-info $basedir");
system("bzr log --limit=1");

my $comb_count = $#$combinations + 1;

my $total = 1;
if ($exhaustive) {
    foreach my $comb_id (0..($comb_count-1)) {
        $total *= $#{$combinations->[$comb_id]}+1;
    }
    if (defined $trials) {
        say("You have specified --run-all-combinations-once, but limited with --trials=$trials");
    }
    doExhaustive(0);
} else {
    doRandom();
}


say("[$$] Summary of various interesting strings from the logs:");
say(Dumper \%results);
foreach my $string ('text=', 'bugcheck', 'Error: assertion', 'mysqld got signal', 'Received signal', 'exception') {
	system("grep -i '$string' $vardir/trial*log");
} 

say("[$$] $0 will exit with exit status $max_result");
exit($max_result);

## ----------------------------------------------------

my $trial_counter = 0;

sub doExhaustive {
    my ($level,@idx) = @_;
    if ($level < $comb_count) {
        my @alts;
        foreach my $i (0..$#{$combinations->[$level]}) {
            push @alts, $i;
        }
        ## Shuffle array
        for (my $i= $#alts;$i>=0;$i--) {
            my $j = $prng->uint16(0, $i);
            my $t = $alts[$i];
            $alts[$i] = $alts[$j];
            $alts[$j] = $t;
        }

        foreach my $alt (@alts) {
            push @idx, $alt;
            if ($trials > 0) {
                doExhaustive($level+1,@idx) if $trial_counter < $trials;
            } else {
                doExhaustive($level+1,@idx);
            }
            pop @idx;
        }
    } else {
        $trial_counter++;
        my @comb;
        foreach my $i (0 .. $#idx) {
            push @comb, $combinations->[$i]->[$idx[$i]];
        }
        my $comb_str = join(' ', @comb);        
        say("Executing Combination ".$trial_counter."/".$total);
        doCombination($trial_counter,$comb_str);
    }
}

## ----------------------------------------------------

sub doRandom {
    foreach my $trial_id (1..$trials) {
        my @comb;
        foreach my $comb_id (0..($comb_count-1)) {
            $comb[$comb_id] = $combinations->[$comb_id]->[$prng->uint16(0, $#{$combinations->[$comb_id]})];
        }
        my $comb_str = join(' ', @comb);        
        doCombination($trial_id,$comb_str);
    }
}

## ----------------------------------------------------
sub doCombination {
    my ($trial_id,$comb_str) = @_;

	my $mask = $prng->uint16(0, 65535);

	my $command = "
		perl ".(defined $ENV{RQG_HOME} ? $ENV{RQG_HOME}."/" : "" )."runall.pl $comb_str
		--queries=100000000
	";

	$command .= " --mask=$mask" if not defined $no_mask;
	$command .= " --duration=$duration" if $duration ne '';
	$command .= " --basedir=$basedir " if $basedir ne '';
	$command .= " --gendata=$gendata " if $gendata ne '';
	$command .= " --grammar=$grammar " if $grammar ne '';
	$command .= " --seed=$seed " if $seed ne '';
	$command .= " --testname=$testname " if $testname ne '';
	$command .= " --xml-output=$xml_output " if $xml_output ne '';
	$command .= " --report-xml-tt" if defined $report_xml_tt;
	$command .= " --report-xml-tt-type=$report_xml_tt_type " if $report_xml_tt_type ne '';
	$command .= " --report-xml-tt-dest=$report_xml_tt_dest " if $report_xml_tt_dest ne '';

	$command .= " --vardir=$vardir/current " if $command !~ m{--mem}sio && $vardir ne '';
	$command =~ s{[\t\r\n]}{ }sgio;
	$command .= " 2>&1 | tee $vardir/trial".$trial_id.'.log';

	$commands[$trial_id] = $command;

	$command =~ s{"}{\\"}sgio;
	$command = 'bash -c "set -o pipefail; '.$command.'"';

	say("[$$] $command");
    my $result = 0;
    $result = system($command) if not $debug;

	$result = $result >> 8;
	say("[$$] runall.pl exited with exit status $result");
	exit($result) if (($result == STATUS_ENVIRONMENT_FAILURE) || ($result == 255)) && (not defined $force);

	if ($result > 0) {
		$max_result = $result >> 8 if ($result >> 8) > $max_result;
		say("[$$] Copying vardir to $vardir/vardir".$trial_id);
		if ($command =~ m{--mem}) {
			system("cp -r /dev/shm/var $vardir/vardir".$trial_id);
		} else {
			system("cp -r $vardir/current $vardir/vardir".$trial_id);
		}
		open(OUT, ">$vardir/vardir".$trial_id."/command");
		print OUT $command;
		close(OUT);
	}
	$results{$result >> 8}++;
}
