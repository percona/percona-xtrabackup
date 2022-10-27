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

#ifndef XTRABACKUP_UTILS_H
#define XTRABACKUP_UTILS_H
#include <my_getopt.h>
namespace xtrabackup {
namespace utils {

/**
  Reads list of options from backup-my.cnf at a given path

  @param [in]  options       list of options to read
  @param [in]  path       path where backup-my.cnf is located

  @return false in case of error, true otherwise
*/
extern bool load_backup_my_cnf(my_option *options, char *path);

/**
  Read server_uuid from backup-my.cnf

  @return false in case of error, true otherwise
*/
bool read_server_uuid();

/* convert the version_str to version
@param[in] version_str version string like 8.0.22.debug
@return version_number like 80022 */
unsigned long get_version_number(std::string version_str);

unsigned long host_total_memory();
unsigned long host_free_memory();

/** Generates uuid
@return uuid string */
std::string generate_uuid();
}  // namespace utils
}  // namespace xtrabackup
#endif  // XTRABACKUP_UTILS_H
