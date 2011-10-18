**********************************
Writing drizzletest test cases
**********************************

Synopsis
========

Here, we discuss various topics related to writing test cases, with a focus on features
that allow for more complex testing scenarios.  Additional documentation for other testing
tools will come later.

Using a pre-populated datadir
=============================
The experimental test runner, dbqp allows for starting a server with a pre-populated datadir
for a test case.  This is accomplished via the use of a .cnf file (versus a master.opt file)
Over time, this will be the direction for all drizzletest cases.

The .cnf file shown below tells the test-runner to use the directory drizzle/tests/std_data/backwards_compat_data
as the datadir for the first server.  If a test uses multiple servers, the .cnf file can have additional sections ([s1]...[sN])::

    [test_servers] 
    servers = [[]]

    [s0]
    load-datadir=backwards_compat_data


All datadirs are expected to be in tests/std_data.  If there is need for the ability to use datadirs outside of this location,
it can be explored.

