/* Copyright (c) 2024, Oracle and/or its affiliates.

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

#ifndef SQL_JOIN_OPTIMIZER_JOIN_DERIVED_KEYS_H
#define SQL_JOIN_OPTIMIZER_JOIN_DERIVED_KEYS_H

class JOIN;
class Query_block;
class THD;
struct AccessPath;

/**
   Collect the set fields of derived tables that are present in predicates
   like "derived_tab.field=function(other_tab.field)". Generate keys for every
   derived_tab+other_tab pair in that set.
   @param thd The current thread.
   @param join The current join.
*/
bool MakeDerivedKeys(THD *thd, JOIN *join);

/**
   Remove any unused keys on derived tables. Update the key number in any
   AccessPath that uses a key that gets shifted to a lower number due to
   those removals.
   @param thd The current thread.
   @param query_block The current Query_block.
   @param root_path The root path of 'query_block'.
*/
void FinalizeDerivedKeys(THD *thd, const Query_block &query_block,
                         AccessPath *root_path);

#endif
