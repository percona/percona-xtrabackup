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

""" mysql_methods
    collection of helper methods (mysqldump, execute_query, etc)
    that make working with a given server easier and repeatable

"""

import os
import subprocess

def execute_command(cmd, stdout_path):
    stdout_file = open(stdout_path,'w')
    cmd_subproc = subprocess.Popen( cmd
                                  , shell=True
                                  , stdout = stdout_file
                                  , stderr = subprocess.STDOUT
                                  )
    cmd_subproc.wait()
    retcode = cmd_subproc.returncode
    close(stdout_file)
    return retcode


def take_mysqldump( server
                  , dump_path = None
                  , databases=[]
                  , tables=[]
                  , dump_path = None
                  , cmd_root = None):
    """ Take a mysqldump snapshot of the given
        server, storing the output to dump_path

    """
    if not dump_path:
        dump_path = os.path.join(server.vardir, 'dumpfile.dat')

    if cmd_root:
        dump_cmd = cmd_root
    else:
        dump_cmd = "%s --no-defaults --user=root --port=%d --host=127.0.0.1 --protocol=tcp" % ( server.mysqldump
                                                                                              , server.master_port
                                                                                              )
        if databases:
            if len(databases) > 1:
                # We have a list of db's that are to be dumped so we handle things
                dump_cmd = ' '.join([dump_cmd, '--databases', ' '.join(databases)])
            else:
               dump_cmd = ' '.join([dump_cmd, databases[0], ' '.join(tables)])

    execute_cmd(dump_cmd, dump_path)

