use strict;

$| = 1;

use lib 'lib';
use GenTest::SimPipe::Testcase;
use GenTest::SimPipe::Oracle::Crash;
use GenTest;
use GenTest::Constants;
use GenTest::Executor::MySQL;
use DBI;
use Data::Dumper;

my $dsn = 'dbi:mysql:port=19300:user=root:host=127.0.0.1:database=test';
my $oracle = GenTest::SimPipe::Oracle::Crash->new( dsn => $dsn , basedir => '/home/philips/bzr/maria-5.3' );
$oracle->startServer();

my $dbh = DBI->connect($dsn, undef, undef, { mysql_multi_statements => 1, RaiseError => 1 });

my $query = "

SELECT alias2.f2
FROM t2 AS alias1
LEFT JOIN t3 AS alias2
LEFT JOIN t4 AS alias3
LEFT JOIN t1 AS alias4 ON alias3.f1 = alias4.f1 JOIN t5 AS alias5 ON alias3.f3 ON alias2.f1 = alias5.f5 ON alias1.f4 = alias2.f4
WHERE alias2.f2 ;
";

$dbh->do("DROP DATABASE IF EXISTS test; CREATE DATABASE test; USE test");

my $test = '/home/philips/bzr/randgen-simpipe/case.test';
open (Q, $test) or die $!;
while (<Q>) {
	chomp;
	next if $_ eq '';
	$dbh->do($_);
}

my $testcase = GenTest::SimPipe::Testcase->newFromDSN( $dsn, [ $query ]);

my $new_testcase = $testcase->simplify($oracle);

print $new_testcase->toString();
