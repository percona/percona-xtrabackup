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


""" server.py:  generic server object used by the server
    manager.  This contains the generic methods for all 
    servers.  Specific types (Drizzle, MySQL, etc) should
    inherit from this guy

"""

# imports
import os

class Server(object):
    """ the server class from which other servers
        will inherit - contains generic methods
        certain methods will be overridden by more
        specific ones

    """

    def __init__(self
                , name
                , server_manager
                , code_tree
                , default_storage_engine
                , server_options
                , requester
                , workdir_root):
        self.skip_keys = [ 'server_manager'
                         , 'system_manager'
                         , 'dirset'
                         , 'preferred_base_port'
                         , 'no_secure_file_priv'
                         , 'secure_file_string'
                         , 'port_block'
                         ]
        self.debug = server_manager.debug
        self.verbose = server_manager.verbose
        self.initial_run = 1
        self.owner = requester
        self.server_options = server_options
        self.default_storage_engine = default_storage_engine
        self.server_manager = server_manager
        # We register with server_manager asap
        self.server_manager.log_server(self, requester)

        self.system_manager = self.server_manager.system_manager
        self.code_tree = code_tree
        self.type = self.code_tree.type
        self.valgrind = self.system_manager.valgrind
        self.gdb = self.system_manager.gdb
        if self.valgrind:
            self.valgrind_time_buffer = 10
        else:
            self.valgrind_time_buffer = 1
        self.cmd_prefix = self.system_manager.cmd_prefix
        self.logging = self.system_manager.logging
        self.no_secure_file_priv = self.server_manager.no_secure_file_priv
        self.name = name
        self.status = 0 # stopped, 1 = running
        self.tried_start = 0
        self.failed_test = 0 # was the last test a failure?  our state is suspect
        self.server_start_timeout = 60 * self.valgrind_time_buffer
        self.pid = None
        self.need_reset = False

    def initialize_databases(self):
        """ Call schemawriter to make db.opt files """
        databases = [ 'test'
                    , 'mysql'
                    ]
        for database in databases:
            db_path = os.path.join(self.datadir,'local',database,'db.opt')
            cmd = "%s %s %s" %(self.schemawriter, database, db_path)
            self.system_manager.execute_cmd(cmd)

    def process_server_options(self):
        """Consume the list of options we have been passed.
           Return a string with them joined

        """
        
        return " ".join(self.server_options)

    def take_db_snapshot(self):
        """ Take a snapshot of our vardir for quick restores """
       
        self.logging.info("Taking clean db snapshot...")
        if os.path.exists(self.snapshot_path):
            # We need to remove an existing path as python shutil
            # doesn't want an existing target
            self.system_manager.remove_dir(self.snapshot_path)
        self.system_manager.copy_dir(self.datadir, self.snapshot_path)

    def restore_snapshot(self):
        """ Restore from a stored snapshot """
        
        #self.logging.verbose("Restoring from db snapshot")
        #self.logging.warning('mysqldir pre-restore:')
        #if os.path.exists(os.path.join(self.datadir,'mysql')):
        #    self.logging.warning(os.listdir(os.path.join(self.datadir,'mysql')))
        if not os.path.exists(self.snapshot_path):
            self.logging.error("Could not find snapshot: %s" %(self.snapshot_path))
        self.system_manager.remove_dir(self.datadir)
        #self.logging.warning('snapshot contents:')
        #self.logging.warning(os.listdir(os.path.join(self.snapshot_path,'mysql')))
        self.system_manager.copy_dir(self.snapshot_path, self.datadir)
        #self.logging.warning('mysqldir post-restore:')
        #self.logging.warning(os.listdir(os.path.join(self.datadir,'mysql')))

    def is_started(self):
        """ Is the server running?  Particulars are server-dependent """

        return "You need to implement is_started"

    def get_start_cmd(self):
        """ Return the command the server_manager can use to start me """

        return "You need to implement get_start_cmd"

    def get_stop_cmd(self):
        """ Return the command the server_manager can use to stop me """

        return "You need to implement get_stop_cmd"

    def get_ping_cmd(self):
        """ Return the command that can be used to 'ping' me 
            Very similar to is_started, but different

            Determining if a server is still running (ping)
            may differ from the method used to determine
            server startup

        """
   
        return "You need to implement get_ping_cmd"

    def cleanup(self):
        """ Cleanup - just free ports for now..."""
        self.system_manager.port_manager.free_ports(self.port_block)

    def set_server_options(self, server_options):
        """ We update our server_options to the new set """
        self.server_options = server_options

    def reset(self):
        """ Voodoo to reset ourselves """
        self.failed_test = 0

    def get_numeric_server_id(self):
        """ Return the integer value of server-id
            Mainly for mysql / percona, but may be useful elsewhere
 
        """

        return int(self.name.split(self.server_manager.server_base_name)[1])
