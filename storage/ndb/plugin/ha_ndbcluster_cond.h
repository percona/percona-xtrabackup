/*
   Copyright (c) 2000, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SQL_HA_NDBCLUSTER_COND_INCLUDED
#define SQL_HA_NDBCLUSTER_COND_INCLUDED

/*
  This file defines the data structures used by engine condition pushdown in
  the NDB Cluster handler
*/

#include "sql/sql_list.h"
#include "storage/ndb/include/ndbapi/NdbApi.hpp"

class Item;
struct key_range;
struct TABLE;
class Ndb_item;
class ha_ndbcluster;

class ha_ndbcluster_cond {
 public:
  ha_ndbcluster_cond(ha_ndbcluster *h);
  ~ha_ndbcluster_cond();

  // Prepare condition for being pushed. Need to call
  // use_cond_push() later to make it available for the handler
  void prep_cond_push(const Item *cond, bool other_tbls_ok);

  // Apply the 'cond_push', pre generate code if possible.
  // Return the pushed condition and the unpushable remainder
  int use_cond_push(const Item *&pushed_cond, const Item *&remainder_cond);

  void cond_clear();
  int generate_scan_filter_from_cond(NdbScanFilter &filter);

  static int generate_scan_filter_from_key(NdbScanFilter &filter,
                                           const class KEY *key_info,
                                           const key_range *start_key,
                                           const key_range *end_key);

  // Get a possibly pre-generated Interpreter code for the pushed condition
  const NdbInterpretedCode &get_interpreter_code() {
    return m_scan_filter_code;
  }

  void set_condition(const Item *cond);
  bool check_condition() const {
    return (m_unpushed_cond == nullptr || eval_condition());
  }

  static void add_read_set(TABLE *table, const Item *cond);
  void add_read_set(TABLE *table) { add_read_set(table, m_unpushed_cond); }

 private:
  int build_scan_filter_predicate(List_iterator<const Ndb_item> &cond,
                                  NdbScanFilter *filter, bool negated) const;
  int build_scan_filter_group(List_iterator<const Ndb_item> &cond,
                              NdbScanFilter *filter, bool negated) const;

  bool eval_condition() const;

  ha_ndbcluster *const m_handler;

  // The serialized pushed condition
  List<const Ndb_item> m_ndb_cond;

  // A pre-generated scan_filter
  NdbInterpretedCode m_scan_filter_code;

 public:
  /**
   * Conditions prepared for pushing by prep_cond_push(), with a possible
   * m_remainder_cond, which is the part of the condition which still has
   * to be evaluated by the mysql server.
   */
  const Item *m_pushed_cond;
  const Item *m_remainder_cond;

 private:
  /**
   * Stores condition which we assumed could be pushed, but too late
   * turned out to be unpushable. (Failed to generate code, or another
   * access methode not allowing push condition selected). In these cases
   * we need to emulate the effect of the (non-)pushed condition by
   * requiring ha_ndbclustet to evaluate 'm_unpushed_cond' before returning
   * only qualifying rows.
   */
  const Item *m_unpushed_cond;
};

#endif
