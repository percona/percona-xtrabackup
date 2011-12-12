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

from percona_tests.innodbCrash.innodbCrashTestCase import innodbCrashTestCase
from percona_tests.innodbCrash import suite_config

server_requirements = suite_config.server_requirements
server_requests = suite_config.server_requests
servers = suite_config.servers 
test_executor = suite_config.test_executor 

class basicTest(innodbCrashTestCase):

    def test_crash(self):
        """
        self.logging = test_executor.logging
        self.servers = servers
        self.master_server = servers[0]
        self.slave_server = servers[1]
        self.randgen_threads = suite_config.randgen_threads  
        self.randgen_queries_per_thread = suite_config.randgen_queries_per_thread 
        self.crashes = suite_config.crashes 
        """
        self.initialize(test_executor, servers, suite_config)

        # create our table
        self.test_bed_cmd = "./gendata.pl --spec=conf/percona/percona_no_blob.zz "
        self.create_test_bed()

        # Our randgen load-generation command (transactional grammar)
        self.test_seq = [  "./gentest.pl"
                        , "--grammar=conf/percona/trx_randDebugCrash.yy"
                        , "--queries=%d" %(self.randgen_queries_per_thread)
                        , "--threads=%d" %(self.randgen_threads)
                        , "--sqltrace"
                        , "--debug"
                        , "--seed=%s" %(self.randgen_seed)
                        ]
        self.test_seq = " ".join(self.test_seq)
        self.execute_crash_test()
