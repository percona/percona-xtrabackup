/******************************************************
* Copyright (c) 2022,2023 Percona LLC and/or its affiliates.

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

/* Changed page tracking implementation */
#include "changed_page_tracking.h"
#include <iostream>
#include "backup_copy.h"
#include "backup_mysql.h"
#include "common.h"
#include "components/mysqlbackup/backup_comp_constants.h"
#include "srv0srv.h"
#include "xb0xb.h"
#include "xtrabackup.h"

namespace pagetracking {

/** @return short form of UUID
@param[in] conn connection handle to MySQL server */
static uint64_t get_uuid_short(MYSQL *connection) {
  char *uuid = read_mysql_one_value(connection, "SELECT UUID_SHORT()");
  uint64_t uuid_short = strtoull(uuid, nullptr, 10);
  free(uuid);
  return (uuid_short);
}

static void wait_till_start_lsn_above_checkpoint(lsn_t start_lsn) {
  debug_sync_point("xtrabackup_after_wait_page_tracking");
  while (true) {
    auto current_checkpoint = log_status_checkpoint_lsn();
    DBUG_EXECUTE_IF("page_tracking_checkpoint_behind", current_checkpoint = 1;
                    DBUG_SET("-d,page_tracking_checkpoint_behind"););
    if (current_checkpoint >= start_lsn) {
      xb::info() << "pagetracking: Checkpoint lsn is "
                 << log_status.lsn_checkpoint
                 << " and page tracking start lsn is " << start_lsn;
      break;
    } else {
      xb::info() << "pagetracking: Sleeping for 1 second, waiting for "
                 << "checkpoint lsn " << log_status.lsn_checkpoint
                 << " to reach to page tracking start lsn " << start_lsn;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

xb_space_map *init(lsn_t page_start_lsn, lsn_t page_end_lsn,
                   MYSQL *connection) {
  const ulong page_tracking_read_buffer_size = ((srv_log_buffer_size) / 2);
  xb_space_map *space_map = nullptr;

  if (page_start_lsn == page_end_lsn) {
    xb::info() << "pagetracking: backup LSN is same as last "
               << "checkpoint LSN " << page_start_lsn
               << " not calling mysql component";
    space_map = new xb_space_map;
    return space_map;
  }

  uint64_t backupid = 0;
  uint64_t total_pages_changed = 0;
  if (!get_changed_pages(page_start_lsn, &backupid, &total_pages_changed,
                         connection)) {
    xb::error() << "pagetracking: Failed to get page tracking file "
                   "from server";
    return nullptr;
  }

  File file{-1};
  char full_path[FN_REFLEN];

  fn_format(
      full_path,
      (std::string(srv_data_home) + FN_LIBCHAR +
       Backup_comp_constants::backup_scratch_dir + FN_LIBCHAR +
       std::to_string(backupid) + Backup_comp_constants::change_file_extension)
          .c_str(),
      "", "", MY_RETURN_REAL_PATH);

  Fil_path::normalize(full_path);

  file = my_open(full_path, O_RDONLY, MYF(MY_WME));

  if (file < 0) {
    xb::error() << "pagetracking: cannot open " << SQUOTE(full_path);
    return nullptr;
  }

  ut::aligned_array_pointer<byte, UNIV_PAGE_SIZE_MAX> buf;
  buf.alloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                    ut::Count{page_tracking_read_buffer_size});
  size_t hdr_len = page_tracking_read_buffer_size;

  space_map = new xb_space_map;

  /* process file to create map of space and it's changed pages */
  while (true) {
    size_t n_read = my_read(file, buf, hdr_len, MYF(MY_WME));

    ut_a(n_read % Backup_comp_constants::page_number_size == 0);

    if (n_read == MY_FILE_ERROR) {
      xb::error() << "pagetracking: cannot read from " << SQUOTE(full_path);
      my_close(file, MYF(MY_FAE));
      return nullptr;
    }

    space_id_t space_id;
    page_no_t page_id;

    auto num_pages = n_read / Backup_comp_constants::page_number_size;

    for (size_t index = 0; index < num_pages; index++) {
      space_id = mach_read_from_4(
          buf + Backup_comp_constants::page_number_size * index);
      page_id =
          mach_read_from_4(buf + Backup_comp_constants::page_number_size / 2 +
                           Backup_comp_constants::page_number_size * index);

      /* insert page_id into existing space or create new space and insert */
      if (space_map->count(space_id)) {
        space_map->at(space_id).insert(page_id);
      } else {
        space_map->insert({space_id, {page_id}});
      }
    }

    if (n_read < hdr_len) {
      break;
    }
  }

  uint64_t total_pages = 0;
  for (auto it = space_map->begin(); it != space_map->end(); it++) {
    total_pages += it->second.pages.size();
  }

  ut_ad(total_pages_changed >= total_pages);

  xb::info() << "pagetracking: Total pages modified: " << total_pages_changed
             << " Duplicate: " << total_pages_changed - total_pages;

  my_close(file, MYF(MY_FAE));
  return space_map;
}

/** Free the tracking map.
@param[in/out] space_map      pagetracking map */
void deinit(xb_space_map *space_map) {
  if (space_map) {
    delete space_map;
  }
}

/* Get the page tracking start lsn
@param[in] connection               MySQL connectionn
@return the page tracking start lsn */
bool is_started(MYSQL *connection) {
  std::string udf_get_start_lsn = Backup_comp_constants::udf_get_start_lsn;
  char *start_lsn_str = read_mysql_one_value(
      connection, ("SELECT " + udf_get_start_lsn + "()").c_str());
  lsn_t page_track_start_lsn = strtoll(start_lsn_str, nullptr, 10);
  free(start_lsn_str);
  return page_track_start_lsn == 0 ? false : true;
}

/* Get the page tracking start lsn
@param[in] connection               MySQL connectionn
@return the page tracking start lsn */
lsn_t get_pagetracking_start_lsn(MYSQL *connection) {
  std::string udf_get_start_lsn = Backup_comp_constants::udf_get_start_lsn;
  char *start_lsn_str = read_mysql_one_value(
      connection, ("SELECT " + udf_get_start_lsn + "()").c_str());
  lsn_t page_track_start_lsn = strtoll(start_lsn_str, nullptr, 10);
  free(start_lsn_str);

  if (page_track_start_lsn == 0) {
    xb::error() << "pagetracking: tracking lsn is 0. Page tracking was "
                   "disabled?";
  }

  return page_track_start_lsn;
}

bool get_changed_pages(lsn_t start_lsn, uint64_t *backupid,
                       uint64_t *changed_pages_count, MYSQL *connection) {
  *backupid = get_uuid_short(connection);

  if (!is_component_installed(connection)) {
    xb::error()
        << "pagetracking: Please install mysqlbackup "
           "component.(INSTALL COMPONENT \"file://component_mysqlbackup\") to "
           "use page tracking";
    return false;
  }

  if (!set_backupid(connection, *backupid)) {
    xb::error() << "pagetracking: unable to set backupid";
    return false;
  }

  /* validate tracking lsn */
  lsn_t page_track_start_lsn = get_pagetracking_start_lsn(connection);

  if (page_track_start_lsn == 0) {
    xb::error() << "pagetracking: page starting start LSN is set to 0";
    return false;
  } else if (page_track_start_lsn > start_lsn) {
    xb::error() << "pagetracking: tracking start lsn " << page_track_start_lsn
                << " is more than requested get pages start lsn " << start_lsn;
    return false;
  }

  /* This check is only required until PS-8710,MySQL 110663 is fixed */
  wait_till_start_lsn_above_checkpoint(start_lsn);

  /* call component api to generate page tracking file */
  xb::info() << "pagetracking: calling get pages from start lsn " << start_lsn
             << "  where current checkpoint LSN "
             << log_status_checkpoint_lsn();

  std::ostringstream query;

  query << "SELECT " << Backup_comp_constants::udf_get_changed_page_count << "("
        << start_lsn << "," << 0 << ")";

  char *get_pages_count_str =
      read_mysql_one_value(connection, query.str().c_str());
  auto get_pages_count = atoi(get_pages_count_str);
  free(get_pages_count_str);
  *changed_pages_count = get_pages_count > 0 ? get_pages_count : 0;

  query.str("");

  query << "SELECT " << Backup_comp_constants::udf_get_changed_pages << "("
        << start_lsn << "," << 0 << ")";

  char *get_pages_str = read_mysql_one_value(connection, query.str().c_str());
  auto get_pages = atoi(get_pages_str);
  free(get_pages_str);

  if (get_pages == 0) {
    return (true);
  } else {
    xb::error() << "pagetracking: failed to generate page tracking "
                << "file with error code " << get_pages;
    return (false);
  }
}

/** Set the backupid
@param[in]   connection  MySQL connection handler
@param[in]   backupid    current backupid
return true if able to set */
bool set_backupid(MYSQL *connection, uint64_t backupid) {
  std::ostringstream query;

  query << "SET GLOBAL " << Backup_comp_constants::mysqlbackup << "."
        << Backup_comp_constants::backupid << "=\"" << backupid << "\"";

  xb_mysql_query(connection, query.str().c_str(), false);

  query.str("");
  query.clear();
  query << "SELECT @@" << Backup_comp_constants::mysqlbackup << "."
        << Backup_comp_constants::backupid;

  char *component_status_str =
      read_mysql_one_value(connection, query.str().c_str());

  uint64_t get_id = strtoull(component_status_str, NULL, 10);
  free(component_status_str);

  if (get_id == backupid) {
    return (true);
  } else {
    xb::error() << "pagetracking: failed to set backupid"
                << " currently it is set to " << get_id;
    return (false);
  }
}

/** Check if mysqlbackup component is installed or not
@param[in]   connection         mysql connection handle
@return true if installed */
bool is_component_installed(MYSQL *connection) {
  char *mysql_component_str =
      read_mysql_one_value(connection,
                           "SELECT COUNT(1) FROM mysql.component WHERE "
                           "component_urn='file://component_mysqlbackup'");

  int mysql_component = strtoull(mysql_component_str, NULL, 10);
  free(mysql_component_str);
  if (mysql_component == 0)
    xb::error()
        << "pagetracking: Please install mysqlbackup "
           "component.(INSTALL COMPONENT \"file://component_mysqlbackup\") to "
           "use page tracking";

  return mysql_component == 0 ? (false) : (true);
}

void range_get_next_page(xb_page_set *page_set) {
  ut_ad(page_set->current_page_it != page_set->pages.end());

  /* loop to find the non continuous page id or end of block */
  while (true) {
    auto current_page = *page_set->current_page_it;
    page_set->current_page_it++;
    if (page_set->current_page_it == page_set->pages.end()) {
      --page_set->current_page_it;
      break;
    }
    auto next_page = *page_set->current_page_it;
    if (next_page != current_page + 1) {
      break;
    }
  }
}

/** Start the page tracking
@param[in]   connection  MySQL connection handler
@param[out]  lsn         lsn at which pagetracking is started
@return true on success. */
bool start(MYSQL *connection, lsn_t *lsn) {
  if (!is_component_installed(connection)) {
    return false;
  }

  std::string udf_set_page_tracking =
      Backup_comp_constants::udf_set_page_tracking;
  char *page_tracking_start_str = read_mysql_one_value(
      connection, ("SELECT " + udf_set_page_tracking + "(1)").c_str());

  *lsn = strtoull(page_tracking_start_str, nullptr, 10);
  auto current_checkpoint_lsn = log_status_checkpoint_lsn();

  if (!is_started(connection)) {
    xb::error() << "pagetracking: Failed to start pagetracking";
    return false;
  }

  xb::info() << "pagetracking: page tracking start LSN: " << *lsn
             << " current checkpoint LSN " << current_checkpoint_lsn;
  free(page_tracking_start_str);

  return true;
}

/** stop the page tracking
@param[in]   connection  MySQL connection handler
@param[out]  lsn         lsn at which pagetracking is stopped
@return true on success. */
bool stop(MYSQL *connection, lsn_t *lsn) {
  if (!is_component_installed(connection)) {
    return false;
  }

  std::string udf_set_page_tracking =
      Backup_comp_constants::udf_set_page_tracking;
  char *page_tracking_start_str = read_mysql_one_value(
      connection, ("SELECT " + udf_set_page_tracking + "(0)").c_str());
  free(page_tracking_start_str);

  *lsn = strtoull(page_tracking_start_str, nullptr, 10);

  if (is_started(connection)) {
    xb::error() << "pagetracking: Failed to stop pagetracking";
    return false;
  }

  xb::info() << "pagetracking: page tracking stop LSN: " << *lsn;

  return true;
}

/** purge the page tracking
@param[in]   connection  MySQL connection handler
@param[in]  lsn         lsn at which pagetracking is stopped
@return true on success. */
bool stop_and_purge(MYSQL *connection) {
  lsn_t stop_lsn;

  stop(connection, &stop_lsn);

  std::string udf_page_track_purge_up_to =
      Backup_comp_constants::udf_page_track_purge_up_to;
  char *page_tracking_stop_str =
      read_mysql_one_value(connection, ("SELECT " + udf_page_track_purge_up_to +
                                        "(" + std::to_string(stop_lsn) + ")")
                                           .c_str());
  free(page_tracking_stop_str);

  auto purge_lsn = strtoull(page_tracking_stop_str, nullptr, 10);
  if (stop_lsn != purge_lsn) {
    xb::error()
        << "pagetracking: could not stop pagetracking. Expected purge LSN "
        << stop_lsn << " server returned " << purge_lsn;
    return false;
  }

  xb::info() << "pagetracking: tracking data purged upto LSN " << stop_lsn;

  return true;
}

}  // namespace pagetracking
