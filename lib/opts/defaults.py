#! /usr/bin/env python
# -*- mode: python; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
#
# Copyright (C) 2010, 2011 Patrick Crews
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



"""store our various default values"""

import os
import sys


def get_defaults(qp_rootdir, project_name):
    """ We store project-variable defaults here
        and return them to seed the runner

    """ 

    # Standard default values
    branch_root = os.path.dirname(qp_rootdir)
    defaults = { 'qp_root':qp_rootdir
               , 'testdir': qp_rootdir
               , 'workdir': os.path.join(qp_rootdir,'workdir')
               , 'basedir': branch_root
               , 'clientbindir': os.path.join(branch_root,'test/server/client')
               , 'server_type':'mysql'
               , 'noshm': False
               , 'valgrind_suppression':os.path.join(qp_rootdir,'valgrind.supp')
               , 'suitepaths': [ os.path.join(branch_root,'plugin')
                           , os.path.join(qp_rootdir,'suite')
                           ]
               , 'suitelist' : [] 
               , 'randgen_path': os.path.join(qp_rootdir,'randgen')
               , 'subunit_file': os.path.join(qp_rootdir,'workdir/test_results.subunit')
               , 'xtrabackuppath': None 
               , 'innobackupexpath': None 
               , 'tar4ibdpath': None 
               , 'wsrep_provider_path':None
               }

    if project_name == 'percona-xtradb-cluster':
        defaults.update( { 'basedir': branch_root
                         , 'clientbindir': os.path.join(branch_root,'/client')
                         , 'server_type':'galera'
                         , 'noshm':True
                         , 'suitepaths': [ os.path.join(qp_rootdir,'percona_tests/') ]
                         , 'suitelist' : ['cluster_basic','cluster_randgen']
                         , 'wsrep_provider_path':'/usr/lib/galera/libgalera_smm.so'
                         })


    if project_name == 'xtrabackup':
        # Xtrabackup tree default values
        defaults.update( { 'basedir': os.path.join(branch_root,'test/server')
                         , 'clientbindir': os.path.join(branch_root,'test/server/client')
                         , 'server_type':'mysql'
                         , 'noshm':True
                         , 'valgrind_suppression':os.path.join(qp_rootdir,'valgrind.supp')
                         , 'suitepaths': [ os.path.join(qp_rootdir,'percona_tests/xtrabackup_main') ] 
                         , 'suitelist' : ['xtrabackup_main']
                         , 'xtrabackuppath': find_xtrabackup_path(branch_root) 
                         , 'innobackupexpath': os.path.join(branch_root,'innobackupex')
                         , 'tar4ibdpath': find_tar4ibd_path(branch_root)
                         })

    return defaults

def find_tar4ibd_path(branch_root):
    """ We scan branch root to locate tar4ibd"""
    for file_name in os.listdir(branch_root):
        if file_name.startswith('libtar') and not file_name.endswith('tar.gz'):
            return os.path.join(branch_root,file_name,'libtar/tar4ibd')

def find_xtrabackup_path(branch_root):
    """ We scan for the xtrabackup binary """
    for file_name in os.listdir(branch_root):
        if file_name.startswith('Percona-Server') or file_name.startswith('mysql'):
            path_options = [ 'storage/innobase/xtrabackup' 
                           , 'storage/innodb_plugin/xtrabackup'
                           ]
            binary_options = [ 'xtrabackup'
                             , 'xtrabackup_51'
                             , 'xtrabackup_55'
                             , 'xtrabackup_innodb55'
                             , 'xtrabackup_plugin'
                             ]
            for path in path_options:
                for binary in binary_options:
                    test_path = os.path.join(branch_root, file_name, path, binary)
                    if os.path.exists(test_path):
                        return test_path
     
