#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2011 Patrick Crews
#
## This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

""" crashme_test_management:
    code related to the gathering / analysis / management of 
    the test cases
    ie - collecting the list of tests in each suite, then
    gathering additional, relevant information for crashme mode

"""

# imports
import os
import re
import sys
import imp

import lib.test_mgmt.test_management as test_management


    
class testCase:
    """Holds info on a single crashme test
 
    """
    def __init__( self
                , system_manager
                , name=None
                , fullname = None
                , server_requirements=[[]]
                , comment=None
                , cnf_path=None
                , request_dict=None
                , test_path = None
                , suitename = 'native_tests'
                , debug=False ):
        self.system_manager = system_manager
        self.logging = self.system_manager.logging
        self.skip_keys = ['system_manager','logging']
        self.name = name
        self.fullname = fullname
        self.suitename = suitename
        self.master_sh = None
        self.comment = comment
        self.server_requirements = server_requirements
        self.cnf_path = cnf_path
        self.server_requests = request_dict
        self.test_path = test_path        
        if debug:
            self.system_manager.logging.debug_class(self)

    def should_run(self):
        if self.skip_flag or self.disable:
            return 0
        else:
            return 1

 
        
        
          
class testManager(test_management.testManager):
    """Deals with scanning test directories, gathering test cases, and 
       collecting per-test information (opt files, etc) for use by the
       test-runner

    """

    def __init__( self, variables, system_manager):
        super(testManager, self).__init__( variables, system_manager)
        server_type = variables['defaultservertype']
        if server_type == 'mysql' or server_type =='galera':  
            server_type = 'percona'
        self.suitepaths = [os.path.join(self.testdir,'%s_tests' %(server_type))]
        if variables['suitelist'] is None:
            self.suitelist = ['main']
        else:
            self.suitelist = variables['suitelist']

    def process_suite(self,suite_dir):
        """Process a test suite.
           Look for tests, which are nice clean python unittest files
        
        """

        # We know this based on how we organize native test conf files
        suite_name = os.path.basename(suite_dir) 
        self.system_manager.logging.verbose("Processing suite: %s" %(suite_name))
        testlist = [os.path.join(suite_dir,test_file) for test_file in sorted(os.listdir(suite_dir)) if test_file.endswith('_test.py')]

        # Look for a suite_conf.py file.  If present, we use that
        # as a source of server_requirements + server_requests barring
        # the presence of test_case level specifications for those values
        suite_conf = os.path.join(suite_dir,'suite_conf.py')
        if not os.path.exists(suite_conf):
            suite_conf = None

        # Search for specific test names
        if self.desired_tests: # We have specific, named tests we want from the suite(s)
           tests_to_use = []
           for test in self.desired_tests:
               if test.endswith('.py'): 
                   pass
               else:
                   test = test+'.py'
               test = os.path.join(suite_dir,test)
               if test in testlist:
                   tests_to_use.append(test)
           testlist = tests_to_use
        for test_case in testlist:
            self.add_test(self.process_test_file( suite_name
                                                , test_case
                                                , suite_conf 
                                                ))

    def get_server_reqs(self, module_file):
        """ Code to handle extraction of server_requests & requirements
            from unittest test modules

        """

        module_name = os.path.basename(module_file).replace('.py','')
        my_module = imp.load_source(module_name, module_file)
        server_requirements, server_requests = None
        try:
            server_requirements = my_module.server_requirements
        except AttributeError, NameError: pass
        try:
            server_requests = test_module.server_requests
        except AttributeError, NameError: pass
         return server_requirements, server_requests


    def process_test_file(self, suite_name, testfile, suite_conf):
        """ We convert the info in a testfile into a testCase object """

        # test_name = filename - .py...simpler
        test_name = os.path.basename(testfile).replace('.py','')
        test_comment = None
        if suite_conf:
            server_requirements, server_requests = self.get_server_reqs(suite_conf)
        # We want to trump any suite-wide configs with test-level ones
        test_server_requirements, test_server_requests = self.get_server_reqs(testfile)
        if test_server_requirements:
            server_requirements = test_server_requirements
        if test_server_requests:
            server_requests = test_server_requests
        
        return testCase( self.system_manager
                       , name = test_name
                       , fullname = "%s.%s" %(suite_name, test_name)
                       , server_requirements = server_requirements
                       , cnf_path = None
                       , request_dict = server_requests
                       , test_path = testfile
                       , debug = self.debug )



    def record_test_result(self, test_case, test_status, output, exec_time):
        """ Accept the results of an executed testCase for further
            processing.
 
        """
        if test_status not in self.executed_tests:
            self.executed_tests[test_status] = [test_case]
        else:
            self.executed_tests[test_status].append(test_case)
        # report
        self.logging.test_report( test_case.fullname, test_status
                                , str(exec_time), output
                                , report_output= True)

