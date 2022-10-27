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

#include "backup_mysql.h"

bool Redo_Log_Consumer::check() {
  if (mysql_server_version <= 80029) return false;

  if (server_flavor == FLAVOR_PERCONA_SERVER) {
    register_query += "(\"PXB\");";
  } else {
    register_query += "();";
  }
  return true;
}

void Redo_Log_Consumer::init(MYSQL *connection) {
  xb_mysql_query(connection, register_query.c_str(), true);
}

void Redo_Log_Consumer::deinit(MYSQL *connection) {
  xb_mysql_query(connection, "DO innodb_redo_log_consumer_unregister()", true);
}

void Redo_Log_Consumer::advance(MYSQL *connection, lsn_t lsn) {
  char query[200];
  snprintf(query, sizeof(query), "DO innodb_redo_log_consumer_advance(%llu);",
           ulonglong{lsn});
  xb_mysql_query(connection, query, true);
}
