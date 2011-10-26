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

def execute_cmd(cmd, exec_path, outfile_path):
    print cmd, '&'*80
    outfile = open(outfile_path,'w')
    cmd_subproc = subprocess.Popen( cmd
                                  , cwd = exec_path
                                  , shell=True
                                  , stdout = outfile 
                                  , stderr = subprocess.STDOUT 
                                  )
    cmd_subproc.wait()
    retcode = cmd_subproc.returncode 
    outfile.close
    in_file = open(outfile_path,'r')
    output = ''.join(in_file.readlines())
    print output
    print '^'*80
    return retcode,output


def innobackupex_backup( innobackupex_path
                       , output_path
                       , server
                       , backup_path
                       , extra_opts=None):
    """ Use the innobackupex binary specified at
        system_manager.innobackupex_path to take
        a backup of the given server

    """

    cmd = "%s --defaults-file=%s--user=root --port=%d --host=127.0.0.1 %s" %( innobackupex_path
                                                                            , server.cnf_file
                                                                            , server.master_port
                                                                            , backup_path)
    if extra_opts:
        cmd = ' '.join([cmd, extra_opts])
    exec_path = os.path.dirname(innobackupex_path)
    retcode = execute_cmd(cmd, exec_path, output_path)
    return retcode

def innobackupex_prepare( innobackupex_path
                        , output_path
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
    exec_path = os.path.dirname(innobackupex_path)
    retcode = execute_cmd(cmd, exec_path, output_path)
    return retcode

def innobackupex_restore( innobackupex_path
                        , output_path
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
    exec_path = os.path.dirname(innobackupex_path)
    retcode = execute_cmd(cmd, exec_path, output_path)
    return retcode




