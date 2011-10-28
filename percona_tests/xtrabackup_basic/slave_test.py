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
import shutil
import unittest

from lib.util.mysql_methods import execute_cmd
from lib.util.mysql_methods import take_mysqldump
from lib.util.mysql_methods import diff_dumpfiles
from lib.util.mysql_methods import execute_query
from lib.util.randgen_methods import execute_randgen


server_requirements = [[],[]]
servers = []
server_manager = None
test_executor = None
# we explicitly use the --no-timestamp option
# here.  We will be using a generic / vanilla backup dir
backup_path = None

class slaveTest(unittest.TestCase):

    def setUp(self):
        master_server = servers[0] # assumption that this is 'master'
        backup_path = os.path.join(master_server.vardir, '_xtrabackup')
        # remove backup path
        if os.path.exists(backup_path):
            shutil.rmtree(backup_path)


    def test_basic1(self):
        innobackupex = test_executor.system_manager.innobackupex_path
        xtrabackup = test_executor.system_manager.xtrabackup_path
        master_server = servers[0] # assumption that this is 'master'
        slave_server = servers[1]
        backup_path = os.path.join(master_server.vardir, '_xtrabackup')
        output_path = os.path.join(master_server.vardir, 'innobackupex.out')
        exec_path = os.path.dirname(innobackupex)
        orig_dumpfile = os.path.join(master_server.vardir,'orig_dumpfile')
        slave_dumpfile = os.path.join(master_server.vardir, 'slave_dumpfile')

        # populate our server with a test bed
        test_cmd = "./gentest.pl --gendata=conf/percona/percona.zz"
        retcode, output = execute_randgen(test_cmd, test_executor, servers)
        
        # take a backup
        cmd = ("%s --defaults-file=%s --user=root --port=%d"
               " --host=127.0.0.1 --no-timestamp --slave-info" 
               " --ibbackup=%s %s" %( innobackupex
                                   , master_server.cnf_file
                                   , master_server.master_port
                                   , xtrabackup
                                   , backup_path))
        retcode, output = execute_cmd(cmd, output_path, exec_path, True)
        self.assertTrue(retcode==0,output)

        # take mysqldump of our current server state
        take_mysqldump(master_server,databases=['test'],dump_path=orig_dumpfile)
        
        # shutdown our server
        server_manager.stop_server(slave_server)

        # prepare our backup
        cmd = ("%s --apply-log --no-timestamp --use-memory=500M "
               "--ibbackup=%s %s" %( innobackupex
                                   , xtrabackup
                                   , backup_path))
        retcode, output = execute_cmd(cmd, output_path, exec_path, True)
        self.assertTrue(retcode==0,output)

        # remove old datadir
        shutil.rmtree(slave_server.datadir)
        os.mkdir(slave_server.datadir)
        
        # restore from backup
        cmd = ("%s --defaults-file=%s --copy-back"
              " --ibbackup=%s %s" %( innobackupex
                                   , slave_server.cnf_file
                                   , xtrabackup
                                   , backup_path))
        retcode, output = execute_cmd(cmd, output_path, exec_path, True)
        self.assertTrue(retcode==0, output)

        # get binlog info for slave 
        slave_file_path = os.path.join(slave_server.datadir,'xtrabackup_binlog_info')
        slave_file = open(slave_file_path,'r')
        binlog_file, binlog_pos = slave_file.readline().strip().split('\t')
        slave_file.close()


        # restart server (and ensure it doesn't crash)
        server_manager.start_server( slave_server
                                   , test_executor
                                   , test_executor.working_environment
                                   , 0)
        self.assertTrue(slave_server.status==1, 'Server failed restart from restored datadir...')

        # update our slave's master info
        query = ("CHANGE MASTER TO "
                 "MASTER_HOST='127.0.0.1',"
                 "MASTER_USER='root',"
                 "MASTER_PASSWORD='',"
                 "MASTER_PORT=%d,"
                 "MASTER_LOG_FILE='%s',"
                 "MASTER_LOG_POS=%d" % ( master_server.master_port
                                       , binlog_file
                                       , int(binlog_pos)))
        retcode, result_set = execute_query(query, slave_server)
        self.assertTrue(retcode==0, result_set)

        # start the slave
        query = "START SLAVE"
        retcode, result_set = execute_query(query, slave_server)
        self.assertTrue(retcode==0, result_set)

        # check the slave status
        query = "SHOW SLAVE STATUS"
        retcode, result_set = execute_query(query, slave_server)
        result_set = result_set[0]
        slave_master_port = result_set[3]
        slave_binlog_file = result_set[5]
        slave_io_running = result_set[10]
        slave_sql_running = result_set[11]
        self.assertEqual(slave_master_port, master_server.master_port)
        self.assertEqual(slave_binlog_file, binlog_file)
        self.assertEqual(slave_io_running, 'Yes')
        self.assertEqual(slave_sql_running, 'Yes')

        # take mysqldump of current server state
        take_mysqldump(slave_server, databases=['test'],dump_path=slave_dumpfile)

        # diff original vs. current server dump files
        retcode, output = diff_dumpfiles(orig_dumpfile, slave_dumpfile)
        self.assertTrue(retcode, output)
 

#    def tearDown(self):
#            server_manager.reset_servers(test_executor.name)


def run_test(output_file):
    suite = unittest.TestLoader().loadTestsFromTestCase(slaveTest)
    return unittest.TextTestRunner(stream=output_file, verbosity=2).run(suite)

