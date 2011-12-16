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

from percona_tests.upgrade.upgradeBaseTestCase import upgradeBaseTestCase 

server_requirements = [[]] 
server_requests = None 
servers = None 
test_executor = None 

class basicTest(upgradeBaseTestCase):

    def test_upgrade(self):
        self.servers = servers
        self.master_server = servers[0]
        self.logging = test_executor.logging

        # Determine our to-be-upgraded datadir
        # based on the server version
        version_string = self.master_server.version[:3]
        # We want to upgrade from a previous version here
        if version_string == '5.1':
            version_string = '5.0' 
        elif version_string == '5.5':
            version_string = '5.1'
            
        self.upgrade_datadir = 'upgrade_data/mysql_%s_blobs' %version_string
        self.show_tables_expected_result = (('A',), ('AA',), ('B',), ('BB',), ('C',), ('CC',), ('D',), ('DD',))
        self.expected_rowcounts = {0: ((0L,),)
                                  ,1:((10L,),)
                                  ,2: ((0L,),)
                                  ,3: ((10L,),)
                                  ,4: ((1L,),)
                                  ,5: ((100L,),)
                                  ,6: ((1L,),)
                                  ,7: ((100L,),)
                                  }

        self.execute_upgrade_test()
 
 
            
        
