# Copyright (C) 2008-2009 Sun Microsystems, Inc. All rights reserved.
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

package GenTest::Generator::FromGrammar;

require Exporter;
@ISA = qw(GenTest::Generator GenTest);

use strict;
use GenTest::Constants;
use GenTest::Random;
use GenTest::Generator;
use GenTest::Grammar;
use GenTest::Grammar::Rule;
use GenTest::Stack::Stack;
use GenTest;
use Cwd;

use constant GENERATOR_MAX_OCCURRENCES	=> 3500;
use constant GENERATOR_MAX_LENGTH	=> 10000;

my $field_pos;
my $cwd = cwd();

sub new {
        my $class = shift;
	my $generator = $class->SUPER::new(@_);

	if (not defined $generator->grammar()) {
#		say("Loading grammar file '".$generator->grammarFile()."' ...");
		$generator->[GENERATOR_GRAMMAR] = GenTest::Grammar->new(
			grammar_file	=> $generator->grammarFile(),
			grammar_string	=> $generator->grammarString()
		);
		return undef if not defined $generator->[GENERATOR_GRAMMAR];
	}

	if (not defined $generator->prng()) {
		$generator->[GENERATOR_PRNG] = GenTest::Random->new(
			seed => $generator->[GENERATOR_SEED] || 0,
			varchar_length => $generator->[GENERATOR_VARCHAR_LENGTH]
		);
	}
        
    if (not defined $generator->maskLevel()) {
        $generator->[GENERATOR_MASK_LEVEL] = 1;    
    }

	$generator->[GENERATOR_SEQ_ID] = 0;

    if ($generator->mask() > 0) {
        my $grammar = $generator->grammar();
        my $top = $grammar->topGrammar($generator->maskLevel(),
                                       "thread".$generator->threadId(),
                                       "query");
        my $maskedTop = $top->mask($generator->mask());
        $generator->[GENERATOR_MASKED_GRAMMAR] = $grammar->patch($maskedTop);
    }

	return $generator;
}

sub globalFrame {
    my ($self) = @_;
    $self->[GENERATOR_GLOBAL_FRAME] = GenTest::Stack::StackFrame->new()
        if not defined $self->[GENERATOR_GLOBAL_FRAME];
    return $self->[GENERATOR_GLOBAL_FRAME];
}

sub participatingRules {
	return $_[0]->[GENERATOR_PARTICIPATING_RULES];
}

#
# Generate a new query. We do this by iterating over the array containing grammar rules and expanding each grammar rule
# to one of its right-side components . We do that in-place in the array.
#
# Finally, we walk the array and replace all lowercase keywors with literals and such.
#

sub next {
	my ($generator, $executors) = @_;
    
	my $grammar = $generator->grammar();
	my $prng = $generator->prng();
	my $mask = $generator->mask();
	my $mask_level = $generator->maskLevel();

    my $stack = GenTest::Stack::Stack->new();
    my $global = $generator->globalFrame();
    
	#
	# If a temporary file has been left from a previous statement, unlink it.
	#

	unlink($generator->[GENERATOR_TMPNAM]) if defined $generator->[GENERATOR_TMPNAM];
	$generator->[GENERATOR_TMPNAM] = undef;

	my $starting_rule;

	# If this is our first query, we look for a rule named "threadN_init" or "query_init"
	if ($generator->seqId() == 0) {
		$starting_rule = 
            $grammar->firstMatchingRule("thread".$generator->threadId()."_init",
                                        "query_init");
		$mask = 0 if defined $starting_rule;
        # Do not apply mask on _init rules.
	}

    ## Apply mask if any
	if (defined $generator->maskedGrammar()) {
        $grammar = $generator->maskedGrammar();
	}

	# If no init starting rule, we look for rules named "threadN" or "query"
	$starting_rule = 
        $grammar->firstMatchingRule("thread".$generator->threadId(),
                                    "query") 
        if not defined $starting_rule;
    
	my @sentence = ($starting_rule);

	my $grammar_rules = $grammar->rules();

	# And we do multiple iterations, continuously expanding grammar rules and replacing the original rule with its expansion.
	
	my %rule_counters;
	my %invariants;

	my $last_table;
	my $last_database;

	my $pos = 0;
	while ($pos <= $#sentence) {
		if ($#sentence > GENERATOR_MAX_LENGTH) {
			say("Sentence is now longer than ".GENERATOR_MAX_LENGTH()." symbols. Possible endless loop in grammar. Aborting.");
			return undef;
		}
		if (ref($sentence[$pos]) eq 'GenTest::Grammar::Rule') {
			splice (@sentence, $pos, 1 , map {

				$rule_counters{$_}++ if $_ ne uc($_);

				if ($rule_counters{$_} > GENERATOR_MAX_OCCURRENCES) {
					say("Rule $_ occured more than ".GENERATOR_MAX_OCCURRENCES()." times. Possible endless loop in grammar. Aborting.");
					return undef;
				}

				# Check if we just picked a grammar rule. If yes, then return its Rule object.	
				# If not, use the original literal, stored in $_

				if (exists $grammar_rules->{$_}) {
					$grammar_rules->{$_};
				} else {
					$_;
				}
			} @{$prng->arrayElement($sentence[$pos]->[GenTest::Grammar::Rule::RULE_COMPONENTS])});
			if ($@ ne '') {
				say("Internal grammar problem: $@");
				return undef;
			}
		} else {
			$pos++;
		}
	}

	# Once the SQL sentence has been constructed, iterate over it to replace variable items with their final values
	
	my $item_nodash;
	my $orig_item;
	foreach (@sentence) {
		$orig_item = $_;
		next if $_ eq ' ';

		if (
			($_ =~ m{^\{}so) &&
			($_ =~ m{\}$}so)
		) {
			$_ = eval("no strict;\n".$_);		# Code

			if ($@ =~ m{at \(.*?\) line}o) {
				say("Internal grammar error: $@");
				return undef;			# Code called die()
			} elsif ($@ ne '') {
				warn("Syntax error in Perl snippet $orig_item : $@");
				return undef;
			}
			next;
		} elsif ($_ =~ m{^\$}so) {
			$_ = eval("no strict;\n".$_.";\n");	# Variable
			next;
		}

		my $modifier;

		my $invariant_substitution=0;
		if ($_ =~ m{^(_[a-z_]*?)\[(.*?)\]}sio) {
			$modifier = $2;
			if ($modifier eq 'invariant') {
				$invariant_substitution=1;
				$_ = exists $invariants{$orig_item} ? $invariants{$orig_item} : $1 ;
			} else {
				$_ = $1;
			}
		}

		next if $_ eq uc($_);				# Short-cut for UPPERCASE literals

		if ( ($_ eq 'letter') || ($_ eq '_letter') ) {
			$_ = $prng->letter();
		} elsif ($_ eq '_hex') {
			$_ = $prng->hex();
		} elsif ( ($_ eq 'digit')  || ($_ eq '_digit') ) {
			$_ = $prng->digit();
		} elsif ($_ eq '_cwd') {
			$_ = "'".$cwd."'";
		} elsif (
			($_ eq '_tmpnam') ||
			($_ eq 'tmpnam') ||
			($_ eq '_tmpfile')
		) {
			# Create a new temporary file name and record it for unlinking at the next statement
			$generator->[GENERATOR_TMPNAM] = tmpdir()."gentest".$$.".tmp" if not defined $generator->[GENERATOR_TMPNAM];
			$_ = "'".$generator->[GENERATOR_TMPNAM]."'";
			$_ =~ s{\\}{\\\\}sgio if osWindows();	# Backslash-escape backslashes on Windows
		} elsif ($_ eq '_tmptable') {
			$_ = "tmptable".$$;
		} elsif ($_ eq '_unix_timestamp') {
			$_ = time();
		} elsif ($_ eq '_pid') {
			$_ = $$;
		} elsif ($_ eq '_thread_id') {
			$_ = $generator->threadId();
		} elsif ($_ eq '_thread_count') {
			$_ = $ENV{RQG_THREADS};
		} elsif (($_ eq '_database') || ($_ eq '_db') || ($_ eq '_schema')) {
			my $databases = $executors->[0]->metaSchemas();
			$last_database = $prng->arrayElement($databases);
			$_ = '`'.$last_database.'`';
		} elsif ($_ eq '_table') {
			my $tables = $executors->[0]->metaTables($last_database);
			$last_table = $prng->arrayElement($tables);
			$_ = '`'.$last_table.'`';
		} elsif ($_ eq '_field') {
			my $fields = $executors->[0]->metaColumns($last_table, $last_database);
			$_ = '`'.$prng->arrayElement($fields).'`';
		} elsif ($_ eq '_field_list') {
			my $fields = $executors->[0]->metaColumns($last_table, $last_database);
			$_ = '`'.join('`,`', @$fields).'`';
		} elsif ($_ eq '_field_count') {
			my $fields = $executors->[0]->metaColumns($last_table, $last_database);
			$_ = $#$fields + 1;
		} elsif ($_ eq '_field_next') {
			# Pick the next field that has not been picked recently and increment the $field_pos counter
			my $fields = $executors->[0]->metaColumns($last_table, $last_database);
			$_ = '`'.$fields->[$field_pos++ % $#$fields].'`';
		} elsif ($_ eq '_field_no_pk') {
			my $fields = $executors->[0]->metaColumnsTypeNot('primary',$last_table, $last_database);
			$_ = '`'.$prng->arrayElement($fields).'`';
		} elsif (($_ eq '_field_indexed') || ($_ eq '_field_key')) {
			my $fields_indexed = $executors->[0]->metaColumnsType('indexed',$last_table, $last_database);
			$_ = '`'.$prng->arrayElement($fields_indexed).'`';
		} elsif (($_ eq '_field_unindexed') || ($_ eq '_field_nokey')) {
			my $fields_unindexed = $executors->[0]->metaColumnsTypeNot('indexed',$last_table, $last_database);
			$_ = '`'.$prng->arrayElement($fields_unindexed).'`';
		} elsif ($_ eq '_collation') {
			my $collations = $executors->[0]->metaCollations();
			$_ = '_'.$prng->arrayElement($collations);
		} elsif ($_ eq '_charset') {
			my $charsets = $executors->[0]->metaCharactersets();
			$_ = '_'.$prng->arrayElement($charsets);
		} elsif ($_ eq '_data') {
			$_ = $prng->file($cwd."/data");
		} elsif (
			($prng->isFieldType($_) == FIELD_TYPE_NUMERIC) ||
			($prng->isFieldType($_) == FIELD_TYPE_BLOB) 
		) {
			$_ = $prng->fieldType($_);
		} elsif ($prng->isFieldType($_)) {
			$_ = $prng->fieldType($_);
			if (($orig_item =~ m{`$}so) || ($_ =~ m{^(b'|0x)}so)) {
				# Do not quote, quotes are already present
			} elsif ($_ =~ m{'}so) {
				$_ = '"'.$_.'"';
			} else {
				$_ = "'".$_."'";
			}
		} elsif ($_ =~ m{^_(.*)}sio) {
			$item_nodash = $1;
			if ($prng->isFieldType($item_nodash)) {
				$_ = "'".$prng->fieldType($item_nodash)."'";
				if ($_ =~ m{'}so) {
					$_ = '"'.$_.'"';
				} else {
					$_ = "'".$_."'";
				}
			}
		}

		# If the grammar initially contained a ` , restore it. This allows
		# The generation of constructs such as `table _digit` => `table 5`

		if (
			($orig_item =~ m{`$}so) && 
			($_ !~ m{`}so)
		) {
			$_ = $_.'`';
		}
	
		$invariants{$orig_item} = $_ if $modifier eq 'invariant';
	}

	$generator->[GENERATOR_SEQ_ID]++;

	my $sentence = join ('', @sentence);

	$generator->[GENERATOR_PARTICIPATING_RULES] = [ keys %rule_counters ];

	# If this is a BEGIN ... END block then send it to server without splitting.
	# Otherwise, split it into individual statements so that the error and the result set from each statement
	# can be examined

	if (
		($sentence =~ m{CREATE}sio) && 
		($sentence =~ m{BEGIN|END}sio)
	) {
		return [ $sentence ];
	} elsif ($sentence =~ m{;}) {
		my @sentences = split (';', $sentence);
		return \@sentences;
	} else {
		return [ $sentence ];
	}
}

1;
