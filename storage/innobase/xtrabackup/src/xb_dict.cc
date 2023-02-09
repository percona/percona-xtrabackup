/******************************************************
Copyright (c) 2023 Percona LLC and/or its affiliates.

interface containing map/set required for PXB

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
#include "xb_dict.h"
#include <memory>
#include "backup_mysql.h"
namespace xb {
std::shared_ptr<dd_tablespaces> build_space_id_set(MYSQL *connection) {
  ut_ad(srv_backup_mode);

  std::shared_ptr<dd_tablespaces> dd_tab = std::make_shared<dd_tablespaces>();
  std::string sql = "SELECT SPACE FROM INFORMATION_SCHEMA.INNODB_TABLESPACES ";

  MYSQL_RES *result = xb_mysql_query(connection, sql.c_str(), true, true);
  if (result) {
    auto rows_counts = mysql_num_rows(result);
    if (rows_counts > 0) {
      MYSQL_ROW row;
      while ((row = mysql_fetch_row(result)) != nullptr) {
        space_id_t space_id = atoi(row[0]);
        dd_tab->insert(space_id);
      }
    } else {
      xb::warn() << " Query " << sql << " did not return any value ";
      return nullptr;
    }
    mysql_free_result(result);
  } else {
    xb::warn() << "Failed to execute query " << sql;
    return nullptr;
  }

  return dd_tab;
}
}  // namespace xb
