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

    def test_alterRenameFK(self):
        self.servers = servers
        master_server = servers[0]
        other_nodes = servers[1:] # this can be empty in theory: 1 node
        time.sleep(5)
        queries = [ ("CREATE TABLE t1(a INT NOT NULL, "
                     "b INT NOT NULL, PRIMARY KEY(a), "
                     "KEY b_key1 (b)) Engine=Innodb " )
                  , ("CREATE TABLE t2(a INT NOT NULL, "
                     "b INT , PRIMARY KEY(a), KEY b_key (b), " 
                     "CONSTRAINT fk_constraint_t2 FOREIGN KEY (b) "
                     "REFERENCES t1(b) ON DELETE SET NULL "
                     "ON UPDATE CASCADE)")
                   ,"ALTER TABLE t1 RENAME TO t1_new_name"
                  ]
        for query in queries:
            retcode, result = self.execute_query(query, master_server)
            self.assertEqual( retcode, 0, result)
        # check 'master'
        query = "SHOW TABLES IN test"
        retcode, master_result_set = self.execute_query(query, master_server)
        self.assertEqual(retcode,0, master_result_set)
        expected_result_set = (('t1_new_name',), ('t2',)) 
        self.assertEqual( master_result_set
                        , expected_result_set
                        , msg = (master_result_set, expected_result_set)
                        )
        query = "SHOW CREATE TABLE t2"
        retcode, master_result = self.execute_query(query, master_server)
        expected_result = ( ( 't2'
                            , ( 'CREATE TABLE `t2` '
                                '(\n  `a` int(11) NOT NULL'
                                ',\n  `b` int(11) DEFAULT NULL'
                                ',\n  PRIMARY KEY (`a`)'
                                ',\n  KEY `b_key` (`b`)'
                                ',\n  CONSTRAINT `fk_constraint_t2` '
                                'FOREIGN KEY (`b`) REFERENCES `t1_new_name` (`b`)'
                                ' ON DELETE SET NULL ON UPDATE CASCADE\n)'
                                ' ENGINE=InnoDB DEFAULT CHARSET=latin1'
                               )
                            ),
                         )
        self.assertEqual( master_result
                        , expected_result
                        , msg = (master_result, expected_result)
                        )
        master_slave_diff = self.check_slaves_by_query( master_server
                                                 , other_nodes
                                                 , query
                                                 , expected_result= master_result)
        self.assertEqual(master_slave_diff, None, master_slave_diff)

