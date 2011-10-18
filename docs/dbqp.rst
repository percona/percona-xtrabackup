**********************************
dbqp
**********************************

Synopsis
========
Drizzle testing tool

**./dbqp** [ *OPTIONS* ] [ TESTCASE ]

Description
===========

:program:`dbqp.py` is BETA software.  It is intended to provide a standardized
platform to facilitate Drizzle testing.  

The default mode is 'dtr' and is used to execute tests from the Drizzle 
test suite.  These tests are included with Drizzle distributions and 
provide a way for users to verify that the system will operate according
to expectations.

The dtr tests use a diff-based paradigm, meaning that the test runner executes
a test and then compares the results received with pre-recorded expected 
results.  In the event of a test failure, the program will provide output
highlighting the differences found between expected and actual results; this
can be useful for troubleshooting and in bug reports.

The program is also integrated with the random query generator testing tool
and a 'rangden' mode is available - it will execute randgen tests when
provided a path to a randgen installation.  Tests are organized similar to dtr
tests, but are .cnf file based.

A 'cleanup' mode is also available as a convenience - it will simply shutdown
any servers that may have been started via start-and-exit.

While most users are concerned with ensuring general functionality, the 
program also allows a user to quickly spin up a server for ad-hoc testing
and to run the test-suite against an already running Drizzle server.

Running tests
=========================

There are several different ways to run tests using :program:`dbqp.py`.

It should be noted that unless :option:`--force` is used, the program will
stop execution upon encountering the first failing test. 
:option:`--force` is recommended if you are running several tests - it will
allow you to view all successes and failures in one run.

Running individual tests
------------------------
If one only wants to run a few, specific tests, they may do so this way::

    ./dbqp.py [OPTIONS] test1 [test2 ... testN]

Running all tests within a suite
--------------------------------
Many of the tests supplied with Drizzle are organized into suites.  

The tests within drizzle/tests/t are considered the 'main' suite.  
Other suites are located in either drizzle/tests/suite or within the various
directories in drizzle/plugin.  Tests for a specific plugin should live in 
the plugin's directory - drizzle/plugin/example_plugin/tests

To run the tests in a specific suite::

    ./dbqp.py [OPTIONS] --suite=SUITENAME

Running specific tests within a suite
--------------------------------------
To run a specific set of tests within a suite::

    ./dbqp.py [OPTIONS] --suite=SUITENAME TEST1 [TEST2..TESTN]

Calling tests using <suitename>.<testname> currently does not work.
One must specify the test suite via the :option:`--suite` option.


Running all available tests
---------------------------
Currently, the quickest way to execute all tests in all suites is
to use 'make test-dbqp' from the drizzle root.

Otherwise, one should simply name all suites::

    ./dbqp.py [OPTIONS] --suite=SUITE1, SUITE2, ...SUITEN

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
  * counts and percentages of total exectuted for all test statuses
  * a listing of failing, skipped, or disabled tests
  * total time spent executing the tests

Example output::

    <snip>
    30 Jan 2011 16:26:31 : main.small_tmp_table                                    [ pass ]           38
    30 Jan 2011 16:26:31 : main.snowman                                            [ pass ]           42
    30 Jan 2011 16:26:31 : main.statement_boundaries                               [ pass ]           47
    30 Jan 2011 16:26:31 : main.status                                             [ pass ]           51
    30 Jan 2011 16:26:31 : main.strict                                             [ pass ]          138
    30 Jan 2011 16:26:43 : main.subselect                                          [ fail ]        12361
    30 Jan 2011 16:26:43 : --- drizzle/tests/r/subselect.result	2011-01-30 16:23:54.975776148 -0500
    30 Jan 2011 16:26:43 : +++ drizzle/tests/r/subselect.reject	2011-01-30 16:26:43.835519303 -0500
    30 Jan 2011 16:26:43 : @@ -5,7 +5,7 @@
    30 Jan 2011 16:26:43 : 2
    30 Jan 2011 16:26:43 : explain extended select (select 2);
    30 Jan 2011 16:26:43 : id	select_type	table	type	possible_keys	key	key_len	ref	rows	filtered	Extra
    30 Jan 2011 16:26:43 : -9	PRIMARY	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
    30 Jan 2011 16:26:43 : +1	PRIMARY	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	No tables used
    <snip>
    30 Jan 2011 16:30:20 : ================================================================================
    30 Jan 2011 16:30:20 INFO: Test execution complete in 314 seconds
    30 Jan 2011 16:30:20 INFO: Summary report:
    30 Jan 2011 16:30:20 INFO: Executed 552/552 test cases, 100.00 percent
    30 Jan 2011 16:30:20 INFO: STATUS: FAIL, 1/552 test cases, 0.18 percent executed
    30 Jan 2011 16:30:20 INFO: STATUS: PASS, 551/552 test cases, 99.82 percent executed
    30 Jan 2011 16:30:20 INFO: FAIL tests: main.subselect
    30 Jan 2011 16:30:20 INFO: Spent 308 / 314 seconds on: TEST(s)
    30 Jan 2011 16:30:20 INFO: Test execution complete
    30 Jan 2011 16:30:20 INFO: Stopping all running servers...

    
Additional uses
===============
Starting a server for manual testing
------------------------------------

:program:`dbqp.py` allows a user to get a Drizzle server up and running
quickly.  This can be useful for fast ad-hoc testing.

To do so call::

    ./dbqp.py --start-and-exit [*OPTIONS*]

This will start a Drizzle server that you can connect to and query

Starting a server against a pre-populated DATADIR
--------------------------------------------------

Using :option:`--start-dirty` prevents :program:`dbqp.py` from attempting
to initialize (clean) the datadir.  This can be useful if you want to use
an already-populated datadir for testing.

NOTE: This feature is still being tested, use caution with your data!!!

Randgen mode / Executing randgen tests
---------------------------------------

Using :option:`--mode` =randgen and :option:`--randgen-path` =/path/to/randgen
will cause the randgen tests to execute.  This are simple .cnf file-based
tests that define various randgen command lines that are useful in testing
the server.  Test organization is similar to the dtr tests.  Tests live in 
suites, the default suite is 'main' and they all live in
drizzle/tests/randgen_tests::

	./dbqp.py --mode=randgen --randgen-path=/path/to/randgen

A user may specify suites and individual tests to run, just as with dtr-based
testing.  Test output is the same as well::

    ./dbqp --mode=randgen --randgen-path=/home/username/repos/randgen
    Setting --no-secure-file-priv=True for randgen mode...
    <snip>
    23 Feb 2011 11:42:43 INFO: Using testing mode: randgen
    <snip>
    23 Feb 2011 11:44:58 : ================================================================================
    23 Feb 2011 11:44:58 : TEST NAME                                               [ RESULT ]    TIME (ms)
    23 Feb 2011 11:44:58 : ================================================================================
    23 Feb 2011 11:44:58 : main.optimizer_subquery                                 [ pass ]       134153
    23 Feb 2011 11:45:03 : main.outer_join                                         [ pass ]         5136
    23 Feb 2011 11:45:06 : main.simple                                             [ pass ]         2246
    23 Feb 2011 11:45:06 : ================================================================================
    23 Feb 2011 11:45:06 INFO: Test execution complete in 142 seconds
    23 Feb 2011 11:45:06 INFO: Summary report:
    23 Feb 2011 11:45:06 INFO: Executed 3/3 test cases, 100.00 percent
    23 Feb 2011 11:45:06 INFO: STATUS: PASS, 3/3 test cases, 100.00 percent executed
    23 Feb 2011 11:45:06 INFO: Spent 141 / 142 seconds on: TEST(s)
    23 Feb 2011 11:45:06 INFO: Test execution complete
    23 Feb 2011 11:45:06 INFO: Stopping all running servers...

Cleanup mode
-------------
A cleanup mode is provided for user convenience.  This simply shuts down
any servers whose pid files are detected in the dbqp workdir.  It is mainly
intended as a quick cleanup for post-testing with :option:`--start-and-exit`::

	./dbqp.py --mode=cleanup

    Setting --start-dirty=True for cleanup mode...
    23 Feb 2011 11:35:59 INFO: Using Drizzle source tree:
    23 Feb 2011 11:35:59 INFO: basedir: drizzle
    23 Feb 2011 11:35:59 INFO: clientbindir: drizzle/client
    23 Feb 2011 11:35:59 INFO: testdir: drizzle/tests
    23 Feb 2011 11:35:59 INFO: server_version: 2011.02.2188
    23 Feb 2011 11:35:59 INFO: server_compile_os: unknown-linux-gnu
    23 Feb 2011 11:35:59 INFO: server_platform: x86_64
    23 Feb 2011 11:35:59 INFO: server_comment: (Source distribution (dbqp_randgen))
    23 Feb 2011 11:35:59 INFO: Using --start-dirty, not attempting to touch directories
    23 Feb 2011 11:35:59 INFO: Using default-storage-engine: innodb
    23 Feb 2011 11:35:59 INFO: Using testing mode: cleanup
    23 Feb 2011 11:35:59 INFO: Killing pid 10484 from drizzle/tests/workdir/testbot0/server0/var/run/server0.pid
    23 Feb 2011 11:35:59 INFO: Stopping all running servers...

Program architecture
====================

:program:`dbqp.py`'s 'dtr' mode uses a simple diff-based mechanism for testing.
This is the default mode and where the majority of Drizzle testing occurs.  
It will execute the statements contained in a test and compare the results 
to pre-recorded expected results.  In the event of a test failure, you
will be presented with a diff::

    main.exp1                                                    [ fail ]
    --- drizzle/tests/r/exp1.result	2010-11-02 02:10:25.107013998 +0300
    +++ drizzle/tests/r/exp1.reject	2010-11-02 02:10:32.017013999 +0300
    @@ -5,4 +5,5 @@
    a
    1
    2
    +3
    DROP TABLE t1;

A test case consists of a .test and a .result file.  The .test file includes
the various statements to be executed for a test.  The .result file lists
the expected results for a given test file.  These files live in tests/t 
and tests/r, respectively.  This structure is the same for all test suites.

dbqp.py options
===================

The :program:`dbqp.py` tool has several available options:

./dbqp.py [ OPTIONS ] [ TESTCASE ]


Options
-------

.. program:: dbqp.py

.. option:: -h, --help
 
   show this help message and exit

Options for the test-runner itself
----------------------------------

.. program:: dbqp.py

.. option:: --force

    Set this to continue test execution beyond the first failed test

.. option:: --start-and-exit

   Spin up the server(s) for the first specified test then exit 
   (will leave servers running)

.. option:: --verbose

   Produces extensive output about test-runner state.  
   Distinct from --debug

.. option:: --debug

   Provide internal-level debugging output.  
   Distinct from --verbose

.. option:: --mode=MODE

   Testing mode.  
   We only support dtr...for now >;) 
   [dtr]

.. option:: --record

   Record a testcase result 
   (if the testing mode supports it) 
   [False]

.. option:: --fast

   Don't try to cleanup from earlier runs 
   (currently just a placeholder) [False]

.. option:: --randgen-path=RANDGENPATH

    The path to a randgen installation that can be used to
    execute randgen-based tests


Options for controlling which tests are executed
------------------------------------------------

.. program:: dbqp.py

.. option:: --suite=SUITELIST

   The name of the suite containing tests we want. 
   Can accept comma-separated list (with no spaces). 
   Additional --suite args are appended to existing list 
   [autosearch]

.. option:: --suitepath=SUITEPATHS 

   The path containing the suite(s) you wish to execute. 
   Use on --suitepath for each suite you want to use.

.. option:: --do-test=DOTEST

   input can either be a prefix or a regex. 
   Will only execute tests that match the provided pattern

.. option:: --skip-test=SKIPTEST

   input can either be a prefix or a regex.  
   Will exclude tests that match the provided pattern

.. option:: --reorder

   sort the testcases so that they are executed optimally
   for the given mode [False]

.. option:: --repeat=REPEAT     

    Run each test case the specified number of times.  For
    a given sequence, the first test will be run n times,
    then the second, etc [1]

Options for defining the code that will be under test
-----------------------------------------------------

.. program:: dbqp.py

.. option:: --basedir=BASEDIR   

   Pass this argument to signal to the test-runner 
   that this is an in-tree test (not required).  
   We automatically set a number of variables 
   relative to the argument (client-bindir, 
   serverdir, testdir) [../]

.. option:: --serverdir=SERVERPATH

   Path to the server executable.  [auto-search]

.. option:: --client-bindir=CLIENTBINDIR

   Path to the directory containing client program
   binaries for use in testing [auto-search]

.. option:: --default-storage-engine=DEFAULTENGINE
                        
   Start drizzled using the specified engine [innodb]

Options for defining the testing environment
--------------------------------------------

.. program:: dbqp.py

.. option:: --testdir=TESTDIR   

    Path to the test dir, containing additional files for
    test execution. [pwd]

.. option:: --workdir=WORKDIR   

   Path to the directory test-run will use to store
   generated files and directories.
   [basedir/tests/dbqp_work]

.. option:: --top-srcdir=TOPSRCDIR

   build option [basedir_default]

.. option:: --top-builddir=TOPBUILDDIR

   build option [basedir_default]

.. option:: --no-shm            

   By default, we symlink workdir to a location in shm.
   Use this flag to not symlink [False]

.. option:: --start-dirty       

   Don't try to clean up working directories before test
   execution [False]

.. option:: --no-secure-file-priv
                        
   Turn off the use of --secure-file-priv=vardir for
   started servers

Options to pass options on to the server
-----------------------------------------

.. program:: dbqp.py

.. option:: --drizzled=DRIZZLEDOPTIONS
           
    Pass additional options to the server.  Will be passed
    to all servers for all tests (mostly for --start-and-
    exit)


Options for defining the tools we use for code analysis (valgrind, gprof, gcov, etc)
------------------------------------------------------------------------------------

.. program:: dbqp.py

.. option:: --valgrind          

   Run drizzletest and drizzled executables using
   valgrind with default options [False]

.. option:: --valgrind-option=VALGRINDARGLIST
                       
   Pass an option to valgrind (overrides/removes default
   valgrind options)

Options for controlling the use of debuggers with test execution
----------------------------------------------------------------

.. program:: dbqp.py

.. option:: --gdb

    Start the drizzled server(s) in gdb

.. option:: --manual-gdb

    Allows you to start the drizzled server(s) in gdb
    manually (in another window, etc

Options to call additional utilities such as datagen
------------------------------------------------------

.. program:: dbqp.py

.. option:: --gendata=GENDATAFILE
            
    Call the randgen's gendata utility to use the
    specified configuration file.  This will populate the
    server prior to any test execution

