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

import signal
import time

from lib.util.mysqlBaseTestCase import mysqlBaseTestCase

class innodbCrashTestCase(mysqlBaseTestCase):
    def initialize(self,test_executor, servers, suite_config):
        self.logging = test_executor.logging
        self.test_executor = test_executor
        self.servers = servers
        self.master_server = servers[0]
        self.slave_server = servers[1]
        self.randgen_threads = suite_config.randgen_threads
        self.randgen_queries_per_thread = suite_config.randgen_queries_per_thread
        self.randgen_seed = test_executor.system_manager.randgen_seed
        self.crashes = suite_config.crashes
        self.kill_db_after = suite_config.kill_db_after

    def create_test_bed(self):
        retcode, output = self.execute_randgen(self.test_bed_cmd, self.test_executor, self.master_server)
        self.assertEqual(retcode,0, output)


    def execute_crash_test(self):
        while self.crashes:
            self.logging.test_debug ("Crashes remaining: %d" %(self.crashes))
            self.crashes -= 1
 
            # generate our workload via randgen
            randgen_process = self.execute_randgen(self.test_seq, self.test_executor, self.master_server)

            if self.master_server.ping(quiet=True):
                # randgen didn't kill the master : (
                self.logging.test_debug ("Killing master manually...")
                self.master_server.die()

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

