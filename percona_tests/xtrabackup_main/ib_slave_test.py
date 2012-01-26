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
import time
import unittest

from lib.util.mysqlBaseTestCase import mysqlBaseTestCase

server_requirements = [[],[]]
servers = []
server_manager = None
test_executor = None
# we explicitly use the --no-timestamp option
# here.  We will be using a generic / vanilla backup dir
backup_path = None

class basicTest(mysqlBaseTestCase):

    def setUp(self):
        master_server = servers[0] # assumption that this is 'master'
        backup_path = os.path.join(master_server.vardir, '_xtrabackup')
        # remove backup path
        if os.path.exists(backup_path):
            shutil.rmtree(backup_path)


    def test_basic1(self):
        if servers[0].type not in ['mysql','percona']:
            return
        else:
            self.servers = servers
            innobackupex = test_executor.system_manager.innobackupex_path
            xtrabackup = test_executor.system_manager.xtrabackup_path
            master_server = servers[0] # assumption that this is 'master'
            slave_server = servers[1]
            backup_path = os.path.join(master_server.vardir, '_xtrabackup')
            output_path = os.path.join(master_server.vardir, 'innobackupex.out')
            exec_path = os.path.dirname(innobackupex)

            # populate our server with a test bed
            test_cmd = "./gentest.pl --gendata=conf/percona/percona.zz"
            retcode, output = self.execute_randgen(test_cmd, test_executor, master_server)
            #self.assertEqual(retcode, 0, msg=output)
        
            # take a backup
            cmd = ("%s --defaults-file=%s --user=root --port=%d"
                   " --host=127.0.0.1 --no-timestamp --slave-info" 
                   " --ibbackup=%s %s" %( innobackupex
                                       , master_server.cnf_file
                                       , master_server.master_port
                                       , xtrabackup
                                       , backup_path))
            retcode, output = self.execute_cmd(cmd, output_path, exec_path, True)
            self.assertEqual(retcode, 0, msg = output)

            # shutdown our slave server
            slave_server.stop()

            # prepare our backup
            cmd = ("%s --apply-log --no-timestamp --use-memory=500M "
                   "--ibbackup=%s %s" %( innobackupex
                                       , xtrabackup
                                       , backup_path))
            retcode, output = self.execute_cmd(cmd, output_path, exec_path, True)
            self.assertEqual(retcode, 0, msg = output)

            # remove old datadir
            shutil.rmtree(slave_server.datadir)
            os.mkdir(slave_server.datadir)
        
            # restore from backup
            cmd = ("%s --defaults-file=%s --copy-back"
                  " --ibbackup=%s %s" %( innobackupex
                                       , slave_server.cnf_file
                                       , xtrabackup
                                       , backup_path))
            retcode, output = self.execute_cmd(cmd, output_path, exec_path, True)
            self.assertEqual(retcode,0, msg = output)

            # get binlog info for slave 
            slave_file_path = os.path.join(slave_server.datadir,'xtrabackup_binlog_info')
            slave_file = open(slave_file_path,'r')
            binlog_file, binlog_pos = slave_file.readline().strip().split('\t')
            binlog_file = os.path.basename(binlog_file)
            slave_file.close()

            # restart server (and ensure it doesn't crash)
            slave_server.start()
            self.assertEqual( slave_server.status, 1
                            , msg = 'Server failed restart from restored datadir...')

            # update our slave's master info/ start replication
            retcode, result_set = slave_server.set_master(master_server)
            self.assertEqual(retcode, 0, msg=result_set)

            # check the slave status
            query = "SHOW SLAVE STATUS"
            retcode, result_set = self.execute_query(query, slave_server)
            result_set = result_set[0]
            slave_master_port = result_set[3]
            slave_binlog_file = result_set[5]
            slave_io_running = result_set[10]
            slave_sql_running = result_set[11]
            self.assertEqual(slave_master_port, master_server.master_port, msg=slave_server.dump_errlog())
            self.assertEqual(slave_binlog_file, binlog_file, msg=slave_server.dump_errlog())
            self.assertEqual(slave_io_running, 'Yes', msg=slave_server.dump_errlog())
            self.assertEqual(slave_sql_running, 'Yes', msg=slave_server.dump_errlog())
            self.assertEqual(retcode,0, msg=result_set)

            # compare master/slave states
            result = self.check_slaves_by_checksum(master_server,[slave_server])
            self.assertEqual(result,None,msg=result)

            # create a new table on the master
            query = ("CREATE TABLE t1 "
                     "(col1 int NOT NULL AUTO_INCREMENT PRIMARY KEY )"
                    )
            retcode, result_set = self.execute_query(query, master_server)
            # insert some rows
            query = "INSERT INTO t1 VALUES (),(),(),(),()"
            retcode, result_set = self.execute_query(query, master_server)
            self.assertEqual(retcode,0,msg=result_set)

            # wait a bit for the slave
            # TODO: proper poll routine
            time.sleep(5)
            for query in ["SHOW CREATE TABLE t1"
                          ,"SELECT * FROM t1"]:
                diff = self.check_slaves_by_query(master_server, [slave_server], query)
                self.assertEqual(diff,None,msg=diff)
 

