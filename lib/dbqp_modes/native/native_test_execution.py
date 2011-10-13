#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2011 Patrick Crews
#
#
# This program is free software; you can redistribute it and/or modify
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

""" native_test_execution:
    code related to the execution of native test cases 
    
    We are provided access to a testManager with 
    native-specific testCases.  

"""

# imports
import os
import re
import imp
import sys
import subprocess
import commands

import lib.test_mgmt.test_execution as test_execution
    
class testExecutor(test_execution.testExecutor):
    """ native-mode-specific  executor """

    def execute_testCase (self):
        """ Execute a test module testCase

        """
        test_execution.testExecutor.execute_testCase(self)
        self.status = 0

        # execute test module
        self.execute_test_module()

        # analyze results
        self.current_test_status = self.process_crashme_output()
        self.set_server_status(self.current_test_status)
        self.server_manager.reset_servers(self.name)


    def execute_test_module(self):
        """ Execute the commandline and return the result.
            We use subprocess as we can pass os.environ dicts and whatnot 

        """

        testcase_name = self.current_testcase.fullname
        test_name = self.current_testcase.name
        self.time_manager.start(testcase_name,'test')
        # import our module and pass it some goodies to play with 
        test_module = imp.load_source(test_name, self.current_testcase.test_path)
        self.current_test_retcode = test_module.run_test()
        print self.current_test_retcode, '%'*80
        execution_time = int(self.time_manager.stop(testcase_name)*1000) # millisec
        self.current_test_output = output
        self.current_test_exec_time = execution_time
        if not self.current_test_retcode:
            return 'pass'
        return 'fail'


        
