# Copyright (C) 2009 Sun Microsystems, Inc. All rights reserved.  Use
# is subject to license terms.
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

# Basic grammar test
# Walk through all the grammars and feed them to the Grammar
# constructor. 
#
package ParseAllGrammars;
use base qw(Test::Unit::TestCase);
use lib 'lib';
use GenTest::Grammar;

use Data::Dumper;

sub new {
    my $self = shift()->SUPER::new(@_);
    # your state for fixture here
    return $self;
}

my $generator;
sub set_up {
}

sub tear_down {
    # clean up after test
}

sub test_parse {
    my $self = shift;

    @level1_files = <conf/*/*.yy>;      # ex: conf/examples/example.yy
    @level2_files = <conf/*/*/*.yy>;    # ex: conf/engines/maria/maria_stress.yy
    # Repeat for more levels (or do this in a better way)
    @files = sort((@level1_files, @level2_files));

    foreach $f (@files) {
        print "... $f\n";
        my $grammar = new GenTest::Grammar(grammar_file => $f);
        $self->assert_not_null($grammar, "Grammar was null: $f");

        # Skip further checks of redefine files, recognized by "_redefine." in 
        # file name. These files can contain just a subset of a grammar, so 
        # there is no point looking for a specific grammar rule.
        my $redefine_file = undef;
        if ($f =~ m{_redefine\.}) {
            $redefine_file = 1;
        }
        
        my $startRule = $grammar->firstMatchingRule("query");
        $self->assert_not_null($startRule, '"query" rule was null in '.$f) unless defined $redefine_file;
    }
}


1;
