/* Copyright (c) 2024, 2025, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mysqld_error.h"

#include "sql/json_duality_view/content_tree.h"
#include "sql/json_duality_view/dml.h"
#include "sql/table.h"  // Table_ref

class THD;
class Sql_cmd_insert_base;

namespace jdv {

// API
bool jdv_prepare_insert(THD *, const Table_ref *, Sql_cmd_insert_base *) {
  my_error(ER_JDV_FEATURE_EDITION_LIMIT, MYF(0));
  return true;
}

bool jdv_prepare_update(THD *, const Table_ref *, bool) {
  // Need to reject in prepare to avoid problems when optimizing - e.g.
  // multi-table update with jdv and table.
  my_error(ER_JDV_FEATURE_EDITION_LIMIT, MYF(0));
  return true;
}

bool jdv_prepare_delete(THD *, const Table_ref *, bool) {
  my_error(ER_JDV_FEATURE_EDITION_LIMIT, MYF(0));
  return true;
}

/**
  Entry point called from sql_insert.cc,
  bool Sql_cmd_insert_values::execute_inner(THD *thd);
 */
bool jdv_insert(THD *, const Table_ref *, const mem_root_deque<List_item *> &) {
  assert(false);  // Should have been rejected in prepare
  my_error(ER_JDV_FEATURE_EDITION_LIMIT, MYF(0));
  return true;
}

/**
  Entry point called from sql_update.cc
  bool Sql_cmd_update::update_single_table(THD *thd);
*/
bool jdv_update(THD *, const Table_ref *, const mem_root_deque<Item *> *,
                const mem_root_deque<Item *> *, ulonglong *) {
  assert(false);  // Should have been rejected in prepare
  my_error(ER_JDV_FEATURE_EDITION_LIMIT, MYF(0));
  return true;
}

/**
  Entry point called from sql_delete.cc,
  bool Sql_cmd_delete::delete_from_single_table(THD *thd);
*/
bool jdv_delete(THD *, const Table_ref *, ulonglong *) {
  assert(false);  // Should have been rejected in prepare
  my_error(ER_JDV_FEATURE_EDITION_LIMIT, MYF(0));
  return true;
}

}  // namespace jdv
