**********************************
randgen (random query generator)
**********************************



Description
===========

The randgen aka the random query generator is a database
testing tool.  It uses a grammar-based stochastic model to represent
some desired set of queries (to exercise the optimizer, for example) 
and generates random queries as allowed by the grammar

The primary documentation is here: http://forge.mysql.com/wiki/RandomQueryGenerator

This document is intended to help the user set up their environment so that the tool
may be used in conjunction with the dbqp.py test-runner.  The forge documentation
contains more information on the particulars of the tool itself.

Requirements
============

DBD::drizzle
-------------
The DBD::drizzle module is required it can be found here http://launchpad.net/dbd-drizzle/

Additional information for installing the module::

    Prerequisites
    ----------------
    * Perl
    * Drizzle (bzr branch lp:drizzle)
    * libdrizzle (bzr branch lp:libdrizzle)
    * C compiler

    Installation
    -------------
    You should only have to run the following:

    perl Makefile.PL --cflags=-I/usr/local/drizzle/include/ --libs=-"L/usr/local/drizzle/lib -ldrizzle"


    Depending on where libdrizzle is installed. Also, you'll want to make 
    sure that ldconfig has configured libdrizzle to be in your library path 

Additional information may be found here: http://forge.mysql.com/wiki/RandomQueryGeneratorQuickStart

Installing the randgen
=======================

The code may be branched from launchpad: bzr branch lp:randgen

it also may be downloaded from here http://launchpad.net/randgen/+download

That is all there is : )

Randgen / dbqp tests
====================

These tests are simple .cnf files that can define a few basic variables
that are needed to execute tests.  The most interesting section is test_servers.  It is a simple list of python lists
Each sub-list contains a string of server options that are needed.  Each sub-list represents a server that will be started.
Using an empty sub-list will create a server with the default options::

    [test_info]
    comment = does NOT actually test the master-slave replication yet, but it will.

    [test_command]
    command = ./gentest.pl --gendata=conf/drizzle/drizzle.zz --grammar=conf/drizzle/optimizer_subquery_drizzle.yy --queries=10 --threads=1

    [test_servers]
    servers = [[--innodb.replication-log],[--plugin-add=slave --slave.config-file=$MASTER_SERVER_SLAVE_CONFIG]]

Running tests
=========================

There are several different ways to run tests using :doc:`dbqp` 's randgen mode.

It should be noted that unless :option:`--force` is used, the program will
stop execution upon encountering the first failing test. 
:option:`--force` is recommended if you are running several tests - it will
allow you to view all successes and failures in one run.

Running individual tests
------------------------
If one only wants to run a few, specific tests, they may do so this way::

    ./dbqp --mode=randgen --randgen-path=/path/to/randgen [OPTIONS] test1 [test2 ... testN]

Running all tests within a suite
--------------------------------
Many of the tests supplied with Drizzle are organized into suites.  

The tests within drizzle/tests/randgen_tests/main are considered the 'main' suite.  
Other suites are also subdirectories of drizzle/tests/randgen_tests.

To run the tests in a specific suite::

    ./dbqp --mode=randgen --randgen-path=/path/to/randgen [OPTIONS] --suite=SUITENAME

Running specific tests within a suite
--------------------------------------
To run a specific set of tests within a suite::

    ./dbqp --mode=randgen --randgen-path=/path/to/randgen [OPTIONS] --suite=SUITENAME TEST1 [TEST2..TESTN]

Calling tests using <suitename>.<testname> currently does not work.
One must specify the test suite via the :option:`--suite` option.


Running all available tests
---------------------------
One would currently have to name all suites, but the majority of the working tests live in the main suite
Other suites utilize more exotic server combinations and we are currently tweaking them to better integrate with the 
dbqp system.  The slave-plugin suite does currently have a good config file for setting up simple replication setups for testing.
To execute several suites' worth of tests::

    ./dbqp --mode=randgen --randgen-path=/path/to/randgen [OPTIONS] --suite=SUITE1, SUITE2, ...SUITEN

Interpreting test results
=========================
The output of the test runner is quite simple.  Every test should pass.
In the event of a test failure, please take the time to file a bug here:
*https://bugs.launchpad.net/drizzle*

During a run, the program will provide the user with:
  * test name (suite + name)
  * test status (pass/fail/skipped)
  * time spent executing each test

At the end of a run, the program will provide the user with a listing of:
  * how many tests were run
  * how many tests failed
  * percentage of passing tests
  * a listing of failing tests
  * total time spent executing the tests

Example output::

    24 Feb 2011 17:27:36 : main.outer_join_portable                                [ pass ]         7019
    24 Feb 2011 17:27:39 : main.repeatable_read                                    [ pass ]         2764
    24 Feb 2011 17:28:57 : main.select_stability_validator                         [ pass ]        77946
    24 Feb 2011 17:29:01 : main.subquery                                           [ pass ]         4474
    24 Feb 2011 17:30:52 : main.subquery_semijoin                                  [ pass ]       110355
    24 Feb 2011 17:31:00 : main.subquery_semijoin_nested                           [ pass ]         8750
    24 Feb 2011 17:31:03 : main.varchar                                            [ pass ]         3048
    24 Feb 2011 17:31:03 : ================================================================================
    24 Feb 2011 17:31:03 INFO: Test execution complete in 288 seconds
    24 Feb 2011 17:31:03 INFO: Summary report:
    24 Feb 2011 17:31:03 INFO: Executed 18/18 test cases, 100.00 percent
    24 Feb 2011 17:31:03 INFO: STATUS: PASS, 18/18 test cases, 100.00 percent executed
    24 Feb 2011 17:31:03 INFO: Spent 287 / 288 seconds on: TEST(s)
    24 Feb 2011 17:31:03 INFO: Test execution complete
    24 Feb 2011 17:31:03 INFO: Stopping all running servers...

    
Additional uses
===============
Starting a server for manual testing and (optionally) populating it
--------------------------------------------------------------------

:doc:`dbqp` 's randgen mode allows a user to get a Drizzle server up and running quickly.  This can be useful for fast ad-hoc testing.

To do so call::

    ./dbqp --mode=randgen --randgen-path=/path/to/randgen --start-and-exit [*OPTIONS*]

This will start a Drizzle server that you can connect to and query

With the addition of the --gendata option, a user may utilize the randgen's gendata (table creation and population) tool
to populate a test server.  In the following example, the test server is now populated by the 8 tables listed below::

    ./dbqp --mode=randgen --randgen-path=/randgen --start-and-exit --gendata=/randgen/conf/drizzle/drizzle.zz
    <snip>
    24 Feb 2011 17:48:48 INFO: NAME: server0
    24 Feb 2011 17:48:48 INFO: MASTER_PORT: 9306
    24 Feb 2011 17:48:48 INFO: DRIZZLE_TCP_PORT: 9307
    24 Feb 2011 17:48:48 INFO: MC_PORT: 9308
    24 Feb 2011 17:48:48 INFO: PBMS_PORT: 9309
    24 Feb 2011 17:48:48 INFO: RABBITMQ_NODE_PORT: 9310
    24 Feb 2011 17:48:48 INFO: VARDIR: /home/pcrews/bzr/work/dbqp_randgen_updates/tests/workdir/testbot0/server0/var
    24 Feb 2011 17:48:48 INFO: STATUS: 1
    # 2011-02-24T17:48:48 Default schema: test
    # 2011-02-24T17:48:48 Executor initialized, id GenTest::Executor::Drizzle 2011.02.2198 ()
    # 2011-02-24T17:48:48 # Creating Drizzle table: test.A; engine: ; rows: 0 .
    # 2011-02-24T17:48:48 # Creating Drizzle table: test.B; engine: ; rows: 0 .
    # 2011-02-24T17:48:48 # Creating Drizzle table: test.C; engine: ; rows: 1 .
    # 2011-02-24T17:48:48 # Creating Drizzle table: test.D; engine: ; rows: 1 .
    # 2011-02-24T17:48:48 # Creating Drizzle table: test.AA; engine: ; rows: 10 .
    # 2011-02-24T17:48:48 # Creating Drizzle table: test.BB; engine: ; rows: 10 .
    # 2011-02-24T17:48:48 # Creating Drizzle table: test.CC; engine: ; rows: 100 .
    # 2011-02-24T17:48:49 # Creating Drizzle table: test.DD; engine: ; rows: 100 .
    24 Feb 2011 17:48:49 INFO: User specified --start-and-exit.  dbqp.py exiting and leaving servers running...



