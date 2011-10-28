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
import difflib
import subprocess

import MySQLdb

def execute_cmd(cmd, stdout_path, exec_path=None, get_output=False):
    stdout_file = open(stdout_path,'w')
    cmd_subproc = subprocess.Popen( cmd
                                  , shell=True
                                  , cwd=exec_path
                                  , stdout = stdout_file
                                  , stderr = subprocess.STDOUT
                                  )
    cmd_subproc.wait()
    retcode = cmd_subproc.returncode
    stdout_file.close()
    if get_output:
        data_file = open(stdout_path,'r')
        output = ''.join(data_file.readlines())
    else:
        output = None
    return retcode, output


def take_mysqldump( server
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


def diff_dumpfiles(orig_file_path, new_file_path):
    """ diff two dumpfiles useful for comparing servers """ 
    orig_file = open(orig_file_path,'r')
    restored_file = open(new_file_path,'r')
    orig_file_data = []
    rest_file_data = []
    orig_file_data= filter_data(orig_file.readlines(),'Dump completed')
    rest_file_data= filter_data(restored_file.readlines(),'Dump completed') 
    
    server_diff = difflib.unified_diff( orig_file_data
                                      , rest_file_data
                                      , fromfile=orig_file_path
                                      , tofile=new_file_path
                                      )
    diff_output = []
    for line in server_diff:
        diff_output.append(line)
    output = '\n'.join(diff_output)
    orig_file.close()
    restored_file.close()
    return (diff_output==[]), output

def filter_data(input_data, filter_text ):
    return_data = []
    for line in input_data:
        if filter_text in line.strip():
            pass
        else:
            return_data.append(line)
    return return_data

def execute_query( query
                 , server
                 , server_host = '127.0.0.1'
                 , database_name='test'):
    result_list = []
    try:
        conn = MySQLdb.connect( host = server_host
                              , port = server.master_port
                              , user = 'root'
                              , db = database_name)
        cursor = conn.cursor()
        cursor.execute(query)
        result_set =  cursor.fetchall()
        cursor.close()
    except MySQLdb.Error, e:
        return 1, ("Error %d: %s" %(e.args[0], e.args[1]))
    conn.commit()
    conn.close()
    return 0, result_set
