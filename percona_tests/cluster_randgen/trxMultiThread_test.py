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
import os
import time

from lib.util.mysql_methods import execute_cmd
from lib.util.mysql_methods import execute_query
from lib.util.mysql_methods import check_slaves_by_checksum
from lib.util.randgen_methods import execute_randgen

server_requirements = [[],[],[]]
server_requests = {'join_cluster':[(0,1), (0,2)]}
servers = []
server_manager = None
test_executor = None

class basicTest(unittest.TestCase):

    #def setUp(self):
    #    """ If we need to do anything pre-test, we do it here.
    #        Any code here is executed before any test method we
    #        may execute
    #
    #    """

    #    return


    def test_basic1(self):
        # populate a server with some tables
        master_server = servers[0]
        other_nodes = servers[1:] # this can be empty in theory: 1 node
        time.sleep(5)
        test_cmd = ("./gentest.pl "
                    "--gendata=conf/percona/percona_no_blob.zz "
                    "--grammar=conf/percona/translog_concurrent1.yy "
                    "--threads=10 "
                    "--queries=100 "
                   )
        retcode, output = execute_randgen(test_cmd, test_executor, servers)
        self.assertEqual(retcode, 0, output)
        # check 'master'
        query = "SHOW TABLES IN test"
        retcode, master_result_set = execute_query(query, master_server)
        self.assertEqual(retcode,0, master_result_set)
        expected_result_set = (('A',), ('AA',), ('B',), ('BB',), ('C',), ('CC',), ('D',), ('DD',))
        self.assertEqual( master_result_set
                        , expected_result_set
                        , msg = (master_result_set, expected_result_set)
                        )

        master_slave_diff = check_slaves_by_checksum(master_server, other_nodes) 
        self.assertEqual(master_slave_diff, None, master_slave_diff)
       


        

    def tearDown(self):
            server_manager.reset_servers(test_executor.name)


def run_test(output_file):
    suite = unittest.TestLoader().loadTestsFromTestCase(basicTest)
    return unittest.TextTestRunner(stream=output_file, verbosity=2).run(suite)

