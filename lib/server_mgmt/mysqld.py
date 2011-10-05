#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2010,2011 Patrick Crews
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


""" mysqld.py:  code to allow a serverManager
    to provision and start up a mysqld server object
    for test execution

"""

# imports
import os
import sys
import subproc


from lib.server_mgmt.server import Server

class mysqlServer(Server):
    """ represents a mysql server, its possessions
        (datadir, ports, etc), and methods for controlling
        and querying it

        TODO: create a base server class that contains
              standard methods from which we can inherit
              Currently there are definitely methods / attr
              which are general

    """

    def __init__( self, name, server_manager, code_tree, default_storage_engine
                , server_options, requester, workdir_root):
        super(mysqlServer, self).__init__( name
                                           , server_manager
                                           , code_tree
                                           , default_storage_engine
                                           , server_options
                                           , requester
                                           , workdir_root)
        self.preferred_base_port = 9306
        
        # client files
        self.mysqldump = self.code_tree.mysqldump
        self.mysqladmin = self.code_tree.mysqladmin
        self.mysql_client = self.code_tree.mysql_client
        self.mysqlimport = self.code_tree.mysqlimport
        self.mysqlslap = self.code_tree.mysqlslap
        self.server_path = self.code_tree.mysql_server
        self.mysql_client_path = self.code_tree.mysql_client
      
        # important stuff
        self.langdir = self.code_tree.langdir
        self.charsetdir = self.code_tree.charsetdir
        self.bootstrap_file = self.code_tree.bootstrap_path
        self.bootstrap_cmd = None

        # Get our ports
        self.port_block = self.system_manager.port_manager.get_port_block( self.name
                                                                         , self.preferred_base_port
                                                                         , 1 )
        self.master_port = self.port_block[0]

        # Generate our working directories
        self.dirset = { self.name : { 'var': {'std_data_ln':( os.path.join(self.code_tree.testdir,'std_data'))
                                             ,'log':None
                                             ,'run':None
                                             ,'tmp':None
                                             ,'master-data': {'local': { 'test':None
                                                                       , 'mysql':None
                                                                       }
                                                             }
                                             }  
                                    } 
                      }
        self.workdir = self.system_manager.create_dirset( workdir_root
                                                        , self.dirset)
        self.vardir = os.path.join(self.workdir,'var')
        self.tmpdir = os.path.join(self.vardir,'tmp')
        self.rundir = os.path.join(self.vardir,'run')
        self.logdir = os.path.join(self.vardir,'log')
        self.datadir = os.path.join(self.vardir,'master-data')

        self.error_log = os.path.join(self.logdir,('%s.err' %(self.name)))
        self.bootstrap_log = os.path.join(self.logdir,('bootstrap.log'))
        self.pid_file = os.path.join(self.rundir,('%s.pid' %(self.name)))
        self.socket_file = os.path.join(self.vardir, ('%s.sock' %(self.name)))
        self.timer_file = os.path.join(self.logdir,('timer'))

        self.snapshot_path = os.path.join(self.tmpdir,('snapshot_%s' %(self.master_port)))
        # We want to use --secure-file-priv = $vardir by default
        # but there are times / tools when we need to shut this off
        if self.no_secure_file_priv:
            self.secure_file_string = ''
        else:
            self.secure_file_string = "--secure-file-priv='%s'" %(self.vardir)
        self.user_string = '--user=root'

        self.initialize_databases()
        self.take_db_snapshot()

        self.logging.debug_class(self)

    def report(self):
        """ We print out some general useful info """
        report_values = [ 'name'
                        , 'master_port'
                        , 'mysql_tcp_port'
                        , 'vardir'
                        , 'status'
                        ]
        self.logging.info("%s server:" %(self.owner))
        for key in report_values:
          value = vars(self)[key] 
          self.logging.info("%s: %s" %(key.upper(), value))

    def initialize_databases(self):
        """ Do the voodoo required to have a working database setup.
            For MySQL, this is calling the server with the 
            --bootstrap argument.  We generate the bootstrap
            file during codeTree intialization as the file is standard for
            all MySQL servers that are spawned from a single codeTree

        """

        # generate the bootstrap startup command
        if not self.bootstrap_cmd:
            mysqld_args = [ "--no-defaults"
                          , "--bootstrap"
                          , "--basedir=%s" %(self.code_tree.basedir)
                          , "--datadir=%s" %(self.datadir)
                          , "--loose-skip-falcon"
                          , "--loose-skip-ndbcluster"
                          , "--tmpdir=%s" %(self.tmpdir)
                          , "--core-file"
                          , "--lc-messages-dir=%s" %(self.langdir)
                          , "--character-sets-dir=%s" %(self.charsetdir)
                          ]
            # We add server_path into the mix this way as we
            # may alter how we store / handle server args later
            mysqld_args = [self.server_path].append(mysqld_args)
            self.bootstrap_cmd = " ".join(mysqld_args)
        # execute our command
        bootstrap_subproc = subprocess.Popen( self.bootstrap_cmd
                                            , shell=True
                                            , stdout=self.bootstrap_log
                                            , stderr=self.bootstrap_log
                                            )
        bootstrap_subproc.wait()
        bootstrap_retcode = bootstrap_subproc.returncode
        if bootstrap_retcode:
            self.logging.error("Received retcode: %s executing command: %s"
                               %(bootstrap_retcode, self.bootstrap_cmd))
            self.logging.error("Check the bootstrap log: %s" %(self.bootstrap_log))
            sys.exit(1)


    def get_start_cmd(self):
        """ Return the command string that will start up the server 
            as desired / intended
 
        """

        server_args = [ self.process_server_options()
                      , "--mysql-protocol.port=%d" %(self.master_port)
                      , "--mysql-protocol.connect-timeout=60"
                      , "--innodb.data-file-path=ibdata1:20M:autoextend"
                      , "--sort-buffer-size=256K"
                      , "--max-heap-table-size=1M"
                      , "--mysql-unix-socket-protocol.path=%s" %(self.socket_file)
                      , "--pid-file=%s" %(self.pid_file)
                      , "--mysql-protocol.port=%d" %(self.mysql_tcp_port)
                      , "--default-storage-engine=%s" %(self.default_storage_engine)
                      , "--datadir=%s" %(self.datadir)
                      , "--tmpdir=%s" %(self.tmpdir)
                      , self.secure_file_string
                      , self.user_string
                      ]

        if self.gdb:
            server_args.append('--gdb')
            return self.system_manager.handle_gdb_reqs(self, server_args)
        else:
            return "%s %s %s & " % ( self.cmd_prefix
                                   , self.server_path
                                   , " ".join(server_args)
                                   )


    def get_stop_cmd(self):
        """ Return the command that will shut us down """
        
        return "%s --user=root --port=%d --connect-timeout=5 --silent --password= --shutdown " %(self.mysql_client_path, self.master_port)
           

    def get_ping_cmd(self):
        """Return the command string that will 
           ping / check if the server is alive 

        """

        return "%s --ping --port=%d --user=root" % (self.mysql_client_path, self.master_port)

    def is_started(self):
        """ Determine if the server is up and running - 
            this may vary from server type to server type

        """

        # We experiment with waiting for a pid file to be created vs. pinging
        # This is what test-run.pl does and it helps us pass logging_stats tests
        # while not self.ping_server(server, quiet=True) and timer != timeout:

        return self.system_manager.find_path( [self.pid_file]
                                            , required=0)


