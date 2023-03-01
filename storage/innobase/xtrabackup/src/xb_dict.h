
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
#ifndef XB_DICT_H
#define XB_DICT_H
#include <mysql.h>
#include <memory>
#include <unordered_set>
namespace xb {
/** Tablespace identifier */
typedef uint32_t space_id_t;
using dd_tablespaces = std::unordered_set<space_id_t>;

/* build space_id set using INFORMATION_SCHEMA.INNODB_TABLESPACES
@param[in] connection  MySQL connection handler
@param[out] dd_tabpespaces set containing all space_id present in DD */
std::shared_ptr<dd_tablespaces> build_space_id_set(MYSQL *connection);
}  // namespace xb
#endif
