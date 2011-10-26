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

import os
import shutil
import unittest
import difflib

from lib.util.xtrabackup_methods import innobackupex_backup
from lib.util.xtrabackup_methods import innobackupex_restore
from lib.util.xtrabackup_methods import innobackupex_prepare

from lib.util.randgen_methods import execute_randgen

from lib.util.mysql_methods import take_mysqldump

server_requirements = [[]]
servers = []
server_manager = None
test_executor = None
# we explicitly use the --no-timestamp option
# here.  We will be using a generic / vanilla backup dir
extra_options = '--no-timestamp'
backup_path = None

class basicTest(unittest.TestCase):

    def setUp(self):
        master_server = servers[0] # assumption that this is 'master'
        backup_path = os.path.join(master_server.vardir, '_xtrabackup')
        # remove backup path
        if os.path.exists(backup_path):
            shutil.rm_tree(backup_path)


    def test_basic1(self):
        """ Very basic 'training-wheels' test to ensure
            that a backup taken (under no load) is accurate
            We use a test bed that covers min / max / etc
            of all available data types

        """
        innobackupex = test_executor.system_manager.innobackupex_path
        master_server = servers[0] # assumption that this is 'master'
        backup_path = os.path.join(master_server.vardir, '_xtrabackup')

        # populate our server with a test bed
        test_cmd = "./gentest.pl --gendata=conf/percona/percona.zz"
        retcode, output = execute_randgen(test_cmd, test_executor, servers)
        
        # take a backup
        innobackupex_backup( innobackupex
                           , master_server
                           , backup_path
                           , extra_opts=extra_options)

        # take mysqldump of our current server state
        orig_dumpfile = os.path.join(master_server.vardir,'orig_dumpfile')
        take_mysqldump(master_server,databases=['test'],dump_path=orig_dumpfile)
        
        # shutdown our server
        server_manager.stop_server(master_server)

        # prepare our backup
        innobackupex_prepare( innobackupex
                            , backup_path
                            , extra_opts=extra_options)
        
        # restore from backup
        innobackupex_restore( innobackupex
                            , backup_path
                            , extra_opts=extra_options)

        # restart server (and ensure it doesn't crash)
        server_manager.start_server( master_server
                                   , test_executor
                                   , test_executor.working_environment
                                   , 0)

        # take mysqldump of current server state
        restored_dumpfile = os.path.join(master_server.vardir, 'restored_dumpfile')
        take_mysqldump(master_server, databases=['test'],dump_path=restored_dumpfile)

        # diff original vs. current server dump files
        orig_file = open(orig_dumpfile,'r')
        restored_file = open(restored_dumpfile,'r')
        orig_file_data = [ i for i in orig_file.readlines() if not i.strip().startswith('Dump Completed') ]
        rest_file_data = [ i for i in restored_file.readlines() if not i.strip().startswith('Dump Completed') ]
        server_diff = difflib.unified_diff( orig_file.readlines()
                                          , restored_file.readlines()
                                          , fromfile=orig_dumpfile
                                          , tofile=restored_dumpfile
                                          )
        diff_output = []
        for line in server_diff:
            diff_output.append(line)
        self.assertFalse(diff_output, '\n'.join(diff_output))

    def tearDown(self):
            server_manager.reset_servers(test_executor.name)


def run_test(output_file):
    suite = unittest.TestLoader().loadTestsFromTestCase(basicTest)
    return unittest.TextTestRunner(stream=output_file, verbosity=2).run(suite)

