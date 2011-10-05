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

""" sqlbench_test_execution:
    code related to the execution of sqlbench test cases 
    
    We are provided access to a testManager with 
    sqlbench-specific testCases.  

"""

# imports
import os
import re
import sys
import subprocess
import commands

import lib.test_mgmt.test_execution as test_execution



class testExecutor(test_execution.testExecutor):
    """ sqlbench-specific testExecutor 
        
    """
  
    def execute_testCase (self):
        """ Execute a sqlbench testCase

        """
        test_execution.testExecutor.execute_testCase(self)
        self.status = 0

        # execute sqlbench
        self.execute_sqlbench()

        # analyze results
        self.current_test_status = self.process_sqlbench_output()
        self.set_server_status(self.current_test_status)
        self.server_manager.reset_servers(self.name)
 
    def execute_sqlbench(self):
        """ Execute the commandline and return the result.
            We use subprocess as we can pass os.environ dicts and whatnot 

        """
      
        testcase_name = self.current_testcase.fullname
        self.time_manager.start(testcase_name,'test')
        sqlbench_outfile = os.path.join(self.logdir,'sqlbench.out')
        sqlbench_output = open(sqlbench_outfile,'w')
        sqlbench_cmd = self.current_testcase.test_command
        self.logging.info("Executing sqlbench:  %s" %(sqlbench_cmd))
        
        sqlbench_subproc = subprocess.Popen( sqlbench_cmd
                                         , shell=True
                                         , cwd=os.path.join(self.system_manager.testdir, 'sql-bench')
                                         , env=self.working_environment
                                         , stdout = sqlbench_output
                                         , stderr = subprocess.STDOUT
                                         )
        sqlbench_subproc.wait()
        retcode = sqlbench_subproc.returncode     
        execution_time = int(self.time_manager.stop(testcase_name)*1000) # millisec

        sqlbench_output.close()
        sqlbench_file = open(sqlbench_outfile,'r')
        output = ''.join(sqlbench_file.readlines())
        self.logging.debug(output)
        sqlbench_file.close()

        self.logging.debug("sqlbench_retcode: %d" %(retcode))
        self.current_test_retcode = retcode
        self.current_test_output = output
        self.current_test_exec_time = execution_time

    def process_sqlbench_output(self):
        
        # Check for 'Failed' in sql-bench output
        # The tests don't die on a failed test and
        # require some checking of the output file
        infile_name = self.current_test_output.split('\n')[1].strip()
        inf= open(infile_name, "r")
        inlines= inf.readlines()
        error_flag= False
        for inline in inlines:
            if 'Failed' in inline:
                error_flag= True
                logging.info(inline.strip())
        inf.close()                    
        self.current_test_output += ''.join(inlines)
        if self.current_test_retcode == 0 and not error_flag:
            return 'pass'
        else:
            return 'fail'

