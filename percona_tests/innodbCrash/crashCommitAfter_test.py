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
import sys
import random
import signal
import unittest
import subprocess
import cStringIO
import hashlib
import threading
import commands
import time

import MySQLdb 

from lib.util.mysqlBaseTestCase import mysqlBaseTestCase

server_requirements = [ [ ("--binlog-do-db=test "
                           "--innodb-file-per-table "
                           "--innodb_file_format='Barracuda' "
                           #"--innodb_log_compressed_pages=0 "
                           #"--innodb_background_checkpoint=0 "
                           "--sync_binlog=100 "
                           "--innodb_flush_log_at_trx_commit=2 "
                           )]
                       ,[ ("--innodb_file_format='Barracuda' "
                           #"--innodb_log_compressed_pages=1 "
                           "--innodb_flush_log_at_trx_commit=2"
                          )]
                      ]
server_requests = {'join_cluster':[(0,1)]}
servers = []
server_manager = None
test_executor = None

class basicTest(mysqlBaseTestCase):

    def test_crash(self):
        self.servers = servers
        master_server = servers[0]
        slave_server = servers[1]
        randgen_threads = 5  
        randgen_queries_per_thread = 1000 
        crashes = 10 
        workers = []

        # create our table
        test_cmd = "./gendata.pl --spec=conf/percona/percona_no_blob.zz "
        retcode, output = self.execute_randgen(test_cmd, test_executor, servers[0])
        self.assertEqual(retcode,0, output)

        while crashes:
            print "Crashes remaining: %d" %(crashes)
            crashes -= 1
 
            # generate our workload via randgen
            test_seq = [  "./gentest.pl"
                        , "--grammar=conf/percona/trx_crash_commit_after.yy"
                        , "--queries=%d" %(randgen_queries_per_thread)
                        , "--threads=%d" %(randgen_threads)
                        , "--sqltrace"
                        , "--debug"
                       ]
            test_seq = " ".join(test_seq) 
            randgen_process = self.execute_randgen(test_seq, test_executor, master_server)

            if master_server.ping(quiet=True):
                # randgen didn't kill the master : (
                print "Killing master manually..."           
                master_server.die()

            retcode = master_server.start()
            timeout = 300
            decrement = 1 
            while timeout and not master_server.ping(quiet=True):
                time.sleep(decrement)
                timeout -= decrement
            slave_server.slave_stop()
            slave_server.slave_start()
            master_server.wait_sync_with_slaves([slave_server],timeout=60)
            result = self.check_slaves_by_checksum(master_server,[slave_server])
            self.assertEqual(result,None,msg=result)

