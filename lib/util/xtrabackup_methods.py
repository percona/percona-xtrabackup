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
import subprocess

def execute_command(cmd):
    cmd_subproc = subprocess.Popen( cmd
                                  , shell=True
                                  , stdout = subprocess.STDOUT
                                  , stderr = subprocess.STDOUT
                                  )
    cmd_subproc.wait()
    retcode = cmd_subproc.returncode     
    return retcode


def innobackupex_backup( innobackupex_path
                       , server
                       , backup_path
                       , extra_opts=None):
    """ Use the innobackupex binary specified at
        system_manager.innobackupex_path to take
        a backup of the given server

    """

    cmd = "%s --user=root --password='' --port=%d %s" %( innobackupex_path
                                                       , server.master_port
                                                       , backup_path)
    if extra_opts:
        cmd = ' '.join([cmd, extra_opts])
    retcode = execute_cmd(cmd)
    return retcode

def innobackupex_prepare( innobackupex_path
                        , backup_path
                        , use_mem='500M'
                        , extra_opts=None):
    """ Use innobackupex to prepare an xtrabackup
        backup file

    """
    cmd = "%s --apply-log --use-memory=%s %s" %( innobackupex_path
                                               , use_mem
                                               , backup_path)
    if extra_opts:
        cmd = ' '.join([cmd, extra_opts])
    retcode = execute_cmd(cmd)
    return retcode

def innobackupex_restore( innobackupex_path
                        , backup_path
                        , use_mem='500M'
                        , extra_opts=None):
    """ Use innobackupex to restore a server from
        a prepared xtrabackup backup

    """

    cmd = "%s --copy-back %s" %( innobackupex_path
                               , backup_path
                               )
    if extra_opts:
        cmd = ' '.join([cmd, extra_opts])
    retcode = execute_cmd(cmd)
    return retcode




