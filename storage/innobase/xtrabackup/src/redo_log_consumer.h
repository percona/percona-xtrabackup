/******************************************************
Copyright (c) 2022 Percona LLC and/or its affiliates.

Redo consumer interface.

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

#ifndef XB_REDO_LOG_CONSUMER_H
#define XB_REDO_LOG_CONSUMER_H

#include "backup_mysql.h"

class Redo_Log_Consumer {
 public:
  /* check if server has redo log consumer capability
  @param[in] connection               MySQL connection
  @return true in case of succeess */
  bool check();

  /* register redo log consumer
  @param[in] connection               MySQL connection
  @return true in case of succeess */
  void init(MYSQL *connection);

  /* unregister redo log consumer
  @param[in] connection               MySQL connection */
  void deinit(MYSQL *connection);

  /* advance consumed LSN
  @param[in] connection               MySQL connection
  @param[in] lsn                      lsn number up to copy has been completed
*/
  void advance(MYSQL *connection, lsn_t lsn);

 private:
  std::string register_query = "DO innodb_redo_log_consumer_register";
};  // Redo_Log_Consumer
#endif