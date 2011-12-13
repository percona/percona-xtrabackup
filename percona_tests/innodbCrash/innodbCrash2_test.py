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
import signal
import threading
import time

from percona_tests.innodbCrash.innodbCrashTestCase import innodbCrashTestCase
from percona_tests.innodbCrash import suite_config

server_requirements = suite_config.server_requirements
server_requests = suite_config.server_requests
servers = suite_config.servers 
test_executor = suite_config.test_executor 

class Worker(threading.Thread):
    def __init__( self
                , xid
                , thread_desc
                , time_delay
                , test_executor
                , server
                , logging):
      threading.Thread.__init__(self)
      self.server = server
      self.xid = xid
      self.desc = thread_desc
      self.time_delay = time_delay
      self.test_executor = test_executor
      self.logging = logging
      self.start()

    def finish(self):
        return

    def run(self):
        try:
            self.logging.test_debug( "Will crash after:%d seconds" %(self.time_delay))
            time.sleep(self.time_delay)
            pid = None 
            timeout = self.time_delay*6
            decrement = .25
            while not pid and timeout:
                pid = self.server.get_pid()
                time.sleep(decrement)
                timeout -= decrement
            self.logging.test_debug( "Crashing server: port: %s, pid: %s" %(self.server.master_port, pid))
            try:
                os.kill(int(self.server.pid), signal.SIGKILL)
                self.logging.test_debug( "Killed server pid: %s" %pid)
            except OSError, e:
                  self.logging.test_debug( "Didn't kill server pid: %s" %pid)
                  self.logging.test_debug( e)
        except Exception, e:
            print "caught (%s)" % e
        finally:
            self.finish()

class basicTest(innodbCrashTestCase):
    """ This test case creates a master-slave pair
        then generates a randgen load against the master
        The master server is killed after %kill_db_after seconds
        and restarted.  We restart the slave, then ensure
        the master and slave have matching table checksums once
        they are synced and the test load is stopped

    """

    def test_crash(self):
        self.initialize(test_executor, servers, suite_config)
        workers = []

        # create our table
        self.test_bed_cmd = "./gendata.pl --spec=conf/percona/percona_no_blob.zz "
        self.create_test_bed()

        # generate our workload via randgen
        test_seq = [  "./gentest.pl"
                    , "--grammar=conf/percona/translog_concurrent1.yy"
                    , "--queries=%d" %(self.randgen_queries_per_thread)
                    , "--threads=%d" %(self.randgen_threads)
                    , "--sqltrace"
                    , "--debug"
                    , "--seed=%s" %(self.randgen_seed)
                   ]

        while self.crashes:
            self.logging.test_debug( "Crashes remaining: %d" %(self.crashes))
            self.crashes -= 1
            worker = Worker( 1 
                           , 'time_delay_kill_thread'
                           , self.kill_db_after 
                           , self.test_executor
                           , self.master_server 
                           , self.logging
                           )
            workers.append(worker)
 
            randgen_process = self.get_randgen_process(test_seq, self.test_executor, self.master_server)
            #if not self.master_server.ping(quiet=True) and (randgen_process.poll() is None):
                # Our server is dead, but randgen is running, we kill it to speed up testing
                #randgen_process.send_signal(signal.SIGINT)

            for w in workers:
              w.join()
            time.sleep(2)
            while randgen_process.poll():
                randgen_process.send_signal(signal.SIGINT)

            retcode = self.master_server.start()
            timeout = 300
            decrement = 1 
            while timeout and not self.master_server.ping(quiet=True):
                time.sleep(decrement)
                timeout -= decrement
            self.slave_server.slave_stop()
            self.slave_server.slave_start()
            self.master_server.wait_sync_with_slaves([self.slave_server],timeout=60)
            result = self.check_slaves_by_checksum(self.master_server,[self.slave_server])
            self.assertEqual(result,None,msg=result)

