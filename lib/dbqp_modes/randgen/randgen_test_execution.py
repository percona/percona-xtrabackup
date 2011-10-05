#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2010 Patrick Crews
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

""" dtr_test_execution:
    code related to the execution of dtr test cases 
    
    We are provided access to a testManager with 
    randgen-specific testCases.  We contact the executionManager
    to produce the system and server configurations we need
    to execute a test.

"""

# imports
import os
import sys
import subprocess
import commands

import lib.test_mgmt.test_execution as test_execution

class testExecutor(test_execution.testExecutor):
    """ randgen-specific testExecutor 

    """
  
    def execute_testCase (self):
        """ Execute a randgen testCase

        """
        test_execution.testExecutor.execute_testCase(self)
        self.status = 0

        # execute the randgen
        self.execute_randgen()

        # analyze results
        self.current_test_status = self.process_randgen_output()
        self.set_server_status(self.current_test_status)
        self.server_manager.reset_servers(self.name)
 

    

    def execute_randgen(self):
        """ Execute the commandline and return the result.
            We use subprocess as we can pass os.environ dicts and whatnot 

        """
      
        testcase_name = self.current_testcase.fullname
        self.time_manager.start(testcase_name,'test')
        randgen_outfile = os.path.join(self.logdir,'randgen.out')
        randgen_output = open(randgen_outfile,'w')
        dsn = "--dsn=dbi:drizzle:host=localhost:port=%d:user=root:password="":database=test" %(self.master_server.master_port)
        randgen_cmd = " ".join([self.current_testcase.test_command, dsn])
        randgen_subproc = subprocess.Popen( randgen_cmd
                                         , shell=True
                                         , cwd=self.system_manager.randgen_path
                                         , env=self.working_environment
                                         , stdout = randgen_output
                                         , stderr = subprocess.STDOUT
                                         )
        randgen_subproc.wait()
        retcode = randgen_subproc.returncode     
        execution_time = int(self.time_manager.stop(testcase_name)*1000) # millisec

        randgen_output.close()
        randgen_file = open(randgen_outfile,'r')
        output = ''.join(randgen_file.readlines())
        self.logging.debug(output)
        randgen_file.close()

        self.logging.debug("randgen_retcode: %d" %(retcode))
        self.current_test_retcode = retcode
        self.current_test_output = output
        self.current_test_exec_time = execution_time

    def process_randgen_output(self):
        """ randgen has run, we now check out what we have """
        retcode = self.current_test_retcode
        if retcode == 0:
            return 'pass'
        else:
            return 'fail'

