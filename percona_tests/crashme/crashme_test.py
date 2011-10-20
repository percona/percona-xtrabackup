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

import unittest
import subprocess

from lib.util.crashme_methods import execute_crashme

server_requirements = [[]]
servers = []
server_manager = None
test_executor = None

class crashmeTest(unittest.TestCase):

    #def setUp(self):
    #    """ If we need to do anything pre-test, we do it here.
    #        Any code here is executed before any test method we
    #        may execute
    #
    #    """

    #    return


    def test_runCrashme(self):
        test_cmd = "$SQLBENCH_DIR/crash-me --server=mysqld --host=127.0.0.1 --force --dir=$MYSQL_TEST_WORKDIR  --connect-options=port=$MASTER_MYPORT --verbose --debug --user=root"
        test_status, retcode, output = execute_crashme(test_cmd, test_executor, servers)
        self.assertTrue(retcode==0, output)
        self.assertTrue(test_status=='pass', output)

    def tearDown(self):
            server_manager.reset_servers(test_executor.name)


def run_test(output_file):
    suite = unittest.TestLoader().loadTestsFromTestCase(crashmeTest)
    return unittest.TextTestRunner(stream=output_file, verbosity=2).run(suite)

