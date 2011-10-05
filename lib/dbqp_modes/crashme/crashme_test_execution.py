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

""" crashme_test_execution:
    code related to the execution of crashme test cases 
    
    We are provided access to a testManager with 
    crashme-specific testCases.  

"""

# imports
import os
import re
import sys
import subprocess
import commands

import lib.test_mgmt.test_execution as test_execution
    
class testExecutor(test_execution.testExecutor):
    """ crashme-specific  executor """

    def execute_testCase (self):
        """ Execute a crashme testCase

        """
        test_execution.testExecutor.execute_testCase(self)
        self.status = 0

        self.prepare_config()

        # execute crashme
        self.execute_crashme()

        # analyze results
        self.current_test_status = self.process_crashme_output()
        self.set_server_status(self.current_test_status)
        self.server_manager.reset_servers(self.name)

    def prepare_config(self):
        """ Create the config file crash-me needs to execute """

        output_filename= "%s/drizzle.cfg" % (self.system_manager.workdir)

        # remove the existing configuration file to start fresh
        if os.path.exists(output_filename):
            logging.info("Removing %s" % output_filename)
            os.remove(output_filename)
  
        output_file= open(output_filename,"w")
        # don't support '+' for concatenation
        output_file.writelines("func_extra_concat_as_+=no\n")
        # new boost libraries are causing us to put these limits in, needs investigation
        output_file.writelines("max_text_size=1048576\n")
        output_file.writelines("where_string_size=1048576\n")
        output_file.writelines("select_string_size=1048576\n")
        output_file.flush()
        output_file.close()

    def execute_crashme(self):
        """ Execute the commandline and return the result.
            We use subprocess as we can pass os.environ dicts and whatnot 

        """

        output_filename= "%s/drizzle.cfg" % (self.system_manager.workdir)      
        testcase_name = self.current_testcase.fullname
        self.time_manager.start(testcase_name,'test')
        crashme_outfile = os.path.join(self.logdir,'crashme.out')
        crashme_output = open(crashme_outfile,'w')
        crashme_cmd = self.current_testcase.test_command + " --config-file=%s" %(output_filename)
        self.logging.info("Executing crash-me:  %s" %(crashme_cmd))
        
        crashme_subproc = subprocess.Popen( crashme_cmd
                                         , shell=True
                                         , cwd=os.path.join(self.system_manager.testdir, 'sql-bench')
                                         , env=self.working_environment
                                         , stdout = crashme_output
                                         , stderr = subprocess.STDOUT
                                         )
        crashme_subproc.wait()
        retcode = crashme_subproc.returncode     
        execution_time = int(self.time_manager.stop(testcase_name)*1000) # millisec

        crashme_output.close()
        crashme_file = open(crashme_outfile,'r')
        output = ''.join(crashme_file.readlines())
        self.logging.debug(output)
        crashme_file.close()

        self.logging.debug("crashme_retcode: %d" %(retcode))
        self.current_test_retcode = retcode
        self.current_test_output = output
        self.current_test_exec_time = execution_time

    def process_crashme_output(self):
        if self.current_test_retcode == 0:
            infile_name = self.current_test_output.split('\n')[3].split(':')[1].strip()
            inf= open(infile_name, "r")
            inlines= inf.readlines()
            error_flag= False
            in_error_section = False
            # crash-me is quite chatty and we don't normally want to sift
            # through ALL of that stuff.  We do allow seeing it via --verbose
            if not self.verbose:
                self.current_test_output = ''
            for inline in inlines:
                if in_error_section and not inline.strip().startswith('#'):
                    in_error_section = False
                if '=error' in inline:
                    error_flag= True
                    in_error_section= True
                if in_error_section:
                    self.current_test_output += inline
            inf.close()                
            if not error_flag:
                return 'pass'
        
        return 'fail'

        
