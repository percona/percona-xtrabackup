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

from lib.util.mysql_methods import execute_queries
from lib.util.mysql_methods import execute_query
from lib.util.randgen_methods import execute_randgen
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

class Worker(threading.Thread):
    def __init__( self
                , xid
                , thread_desc
                , time_delay
                , test_executor
                , server):
      threading.Thread.__init__(self)
      self.server = server
      self.xid = xid
      self.desc = thread_desc
      self.time_delay = time_delay
      self.test_executor = test_executor
      self.start()

    def finish(self):
        return

    def run(self):
        try:
            print "Will crash after:%d seconds" %(self.time_delay)
            time.sleep(self.time_delay)
            pid = None 
            timeout = self.time_delay
            decrement = .25
            while not pid and timeout:
                pid = self.server.get_pid()
                time.sleep(decrement)
                timeout -= decrement
            print "Crashing server: port: %s, pid: %s" %(self.server.master_port, pid)
            try:
                os.kill(int(self.server.pid), signal.SIGKILL)
                print "Killed server pid: %d" %(int(self.server.pid))
            except OSError, e:
                  print "Didn't kill server pid: %s" %self.server.pid
                  print e
        except Exception, e:
            print "caught (%s)" % e
        finally:
            self.finish()

class basicTest(mysqlBaseTestCase):

    def test_crash(self):
        self.servers = servers
        master_server = servers[0]
        slave_server = servers[1]
        kill_db_after = 10 
        num_workers =  1 
        randgen_threads = 2   
        randgen_queries_per_thread = 5000 
        crashes = 10 
        workers = []
        server_pid = master_server.pid 

        # create our table
        test_cmd = "./gendata.pl --spec=conf/percona/percona_no_blob.zz "
        retcode, output = self.execute_randgen(test_cmd, test_executor, servers[0])
        self.assertEqual(retcode,0, output)

        while crashes:
            print "Crashes remaining: %d" %(crashes)
            crashes -= 1
            worker = Worker( 1 
                           , 'time_delay_kill_thread'
                           , kill_db_after 
                           , test_executor
                           , master_server )
            workers.append(worker)
 
            # generate our workload via randgen
            test_seq = [  "./gentest.pl"
                        , "--grammar=conf/percona/translog_concurrent1.yy"
                        , "--queries=%d" %(randgen_queries_per_thread)
                        , "--threads=%d" %(randgen_threads)
                        , "--sqltrace"
                        , "--debug"
                       ] 
            randgen_process = self.get_randgen_process(test_seq, test_executor, master_server)
            if not master_server.ping(quiet=True) and (randgen_process.poll() is None):
                # Our server is dead, but randgen is running, we kill it to speed up testing
                randgen_process.send_signal(signal.SIGINT)

            for w in workers:
              w.join()
            while randgen_process.poll():
                randgen_process.send_signal(signal.SIGINT)

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

