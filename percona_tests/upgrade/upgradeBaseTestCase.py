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

from lib.util.mysqlBaseTestCase import mysqlBaseTestCase 


class upgradeBaseTestCase(mysqlBaseTestCase):
    def validate_checksums(self, show_table_result):
        for idx, table_data in enumerate(show_table_result):
            query = "CHECKSUM TABLE %s" %table_data[0]
            retcode, result = self.execute_query(query, self.master_server)
            self.assertEqual(retcode, 0, msg = result)
            self.logging.test_debug(query)
            self.logging.test_debug("%d: %s" %(idx, result))
            self.logging.test_debug('#'*80)
            self.assertEqual(result, self.expected_checksums[idx], msg = ("Expected: %s | Returned: %s" %( self.expected_checksums[idx], result)))

    def validate_row_counts(self, show_table_result):
        for idx, table_data in enumerate(show_table_result):
            query = "SELECT COUNT(*) FROM %s" %(table_data[0])
            retcode, result = self.execute_query(query, self.master_server)
            self.assertEqual(retcode, 0, msg = result)
            self.logging.test_debug(query)
            self.logging.test_debug("%s: %s" %(table_data[0], result))
            self.logging.test_debug('#'*80)
            self.assertEqual(result, self.expected_rowcounts[idx], msg = ("Expected: %s | Returned: %s\n" %( self.expected_rowcounts[idx], result)))

    def execute_upgrade_test(self):

        # Stop the master
        self.logging.test_debug("Stopping master server...")
        self.master_server.stop()

        # Copy our to-be-upgraded datadir
        self.logging.test_debug("Loading upgrade datadir: %s" %self.upgrade_datadir)
        self.master_server.server_manager.load_datadir(self.upgrade_datadir,self.master_server, self.servers)

        # Run mysql-upgrade

        # Restart the master
        self.logging.test_debug("Restarting master...")
        self.master_server.start()

        # Ensure all of our tables are there
        query = "SHOW TABLES IN test"
        retcode, show_table_result = self.execute_query(query, self.master_server)
        self.logging.test_debug(query)
        self.logging.test_debug(show_table_result)
        self.assertEqual(retcode, 0, show_table_result)
        self.assertEqual(show_table_result, self.show_tables_expected_result, msg = ("Expected: %s | "
                                                                                     "Returned: %s" %(self.show_tables_expected_result
                                                                                                     , show_table_result)
                                                                                    ))

        # Validate checksums
        # self.validate_checksums(show_table_result)

        # Validate row_counts
        self.validate_row_counts(show_table_result)

 
