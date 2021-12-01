
/******************************************************
Copyright (c) 2021 Percona LLC and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

*******************************************************/

/* Changed page tracking interface */

#ifndef XB_CHANGED_PAGE_TRACKING_H
#define XB_CHANGED_PAGE_TRACKING_H

#include <fil0fil.h>
#include "common.h"
#include "mysql.h"

namespace pagetracking {
typedef std::set<page_no_t>::iterator page_iterator;

struct xb_page_set {
  std::set<page_no_t> pages;
  xb_page_set(page_no_t first_page) {
    pages.insert(first_page);
    //    current_page_it = pages.begin();
  }
  void insert(page_no_t new_page) { pages.insert(new_page); }
  page_iterator current_page_it;
};

/** All spaces and their modified pages */
typedef std::map<space_id_t, xb_page_set> xb_space_map;

/** Read the disk page tracking file and build the changed page tracking map for
the LSN interval incremental_lsn to checkpoint_lsn_start.
@param[in] checkpoint_lsn_start     start checkpoint lsn
@param[in] connection               MySQL connectionn
@return the built map or nullptr if unable to build for any reason. */
xb_space_map *init(lsn_t checkpoint_lsn_start, MYSQL *connection);

/** Free the tracking map.
@param[in/out] space_map pagetracking map */
void deinit(xb_space_map *space_map);

/** Check if mysqlbackup component is installed or not
@param[in] connection       mysql connection handle
return true if installed */
bool is_component_installed(MYSQL *connection);

/** Move the current_page_it iterator to poin the last page id in current block
@param[in/out] page_set       page_set */
void range_get_next_page(xb_page_set *page_set);

/** Set the backupid
@param[in] connection  MySQL connection handler
@param[in] backupid   Current backupid */
bool set_backupid(MYSQL *connection, uint64_t backupid);

/** Call the mysqlbackup component mysqlbackup_page_track_get_changed_pages
to get the pages between the LSN interval incremental_lsn to
checkpoint_lsn_start. Server writes file havinng spaceid and page in data
directory
@param[in]  start_lsn      start LSN incremental_lsn
@param[in]  end_lsn        end LSN checkpoint_lsn_start
@param[out] backupid       current backupid
@param[in]  connection     MySQL connection handler
@return true or false if failed for any reason. */
bool get_changed_pages(lsn_t start_lsn, lsn_t end_lsn, uint64_t *backupid,
                       MYSQL *connection);

/** Start the page tracking
@param[in]  connection  MySQL connection handler
@param[out] lsn         lsn at which pagetracking is started
@return	true on success. */
bool start(MYSQL *connection, lsn_t *lsn);

}  // namespace pagetracking

#endif
