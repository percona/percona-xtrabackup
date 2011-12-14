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

import time

from lib.util.mysqlBaseTestCase import mysqlBaseTestCase
from percona_tests.innodbCrash import suite_config

server_requirements = suite_config.server_requirements
server_requests = suite_config.server_requests
servers = suite_config.servers 
test_executor = suite_config.test_executor 


class basicTest(mysqlBaseTestCase):

    def test_alterRename_neg(self):
        self.servers = servers
        master_server = servers[0]
        other_nodes = servers[1:] # this can be empty in theory: 1 node
        time.sleep(5)
        queries = [ ( "CREATE TABLE t1(a INT NOT NULL, "
                      "b INT NOT NULL, PRIMARY KEY(a), "
                      "KEY b_key1 (b)) Engine=Innodb " )
                    , "CREATE TABLE t2 LIKE t1"
                  ]
        for query in queries:
            retcode, result = self.execute_query(query, master_server)
            self.assertEqual( retcode, 0, result)
        # check 'master'
        query = "ALTER TABLE t1 RENAME to t2"
        retcode, result_set = self.execute_query(query, master_server)
        expected_result = "Error 1050: Table 't2' already exists"
        self.assertEqual( result_set
                        , expected_result
                        , msg = (result_set, expected_result))
        query = "SHOW TABLES IN test"
        retcode, master_result_set = self.execute_query(query, master_server)
        self.assertEqual(retcode,0, master_result_set)
        expected_result_set = (('t1',),('t2',))
        self.assertEqual( master_result_set
                        , expected_result_set
                        , msg = (master_result_set, expected_result_set)
                        )
        master_slave_diff = self.check_slaves_by_checksum(master_server, other_nodes)
        self.assertEqual(master_slave_diff, None, master_slave_diff)

