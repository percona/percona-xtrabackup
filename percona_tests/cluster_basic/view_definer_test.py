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
import time

from lib.util.mysqlBaseTestCase import mysqlBaseTestCase
from percona_tests.cluster_basic import suite_config

server_requirements = suite_config.server_requirements
server_requests = suite_config.server_requests
servers = suite_config.servers 
test_executor = suite_config.test_executor 


class basicTest(mysqlBaseTestCase):

    def test_basic1(self):
        self.servers = servers
        master_server = servers[0]
        other_nodes = servers[1:] # this can be empty in theory: 1 node
        time.sleep(5)

        # test bed

        queries = [("CREATE TABLE `t1` ("
                    " `a` int(11) NOT NULL AUTO_INCREMENT,"
                    " `b` char(255) DEFAULT NULL,"
                    " PRIMARY KEY (`a`)"
                    ") ENGINE=InnoDB AUTO_INCREMENT=10 DEFAULT CHARSET=latin1"
                   )
                   ,"INSERT INTO `t1` VALUES (1,'TNETENNBA'),(4,'OVERNUMEROUSNESS'),(7,'DID YOU TRY TURNING IT OFF AND ON AGAIN?')"
                   ,"CREATE VIEW t1_view AS SELECT b FROM t1 ORDER BY b desc"
                  ]
        retcode, result = self.execute_queries(queries, master_server)
        self.assertEqual(retcode, 0, msg=result)


        # check 'master'
        query = "SHOW CREATE TABLE t1_view"
        retcode, output = self.execute_query(query, master_server)
        self.assertEqual(retcode,0, output) 
        print output
        expected_result_set = (('t1_view', 'CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`localhost` SQL SECURITY DEFINER VIEW `t1_view` AS select `t1`.`b` AS `b` from `t1` order by `t1`.`b` desc', 'latin1', 'latin1_swedish_ci'),) 
        self.assertEqual( output 
                        , expected_result_set 
                        , msg = (output, expected_result_set)
                        )
        time.sleep(1)
        master_slave_diff = self.check_slaves_by_query(master_server, other_nodes, query, expected_result=expected_result_set)
        self.assertEqual(master_slave_diff, None, master_slave_diff)
        master_slave_diff = self.check_slaves_by_checksum(master_server, other_nodes, schemas=['sakila']) 
        self.assertEqual(master_slave_diff, None, master_slave_diff)
      
    def tearDown(self):  return 
