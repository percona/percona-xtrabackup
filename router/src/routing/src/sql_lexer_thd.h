/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTING_SQL_LEXER_THD_INCLUDED
#define ROUTING_SQL_LEXER_THD_INCLUDED

#include <stdint.h>

#include "my_alloc.h"               // MEM_ROOT
#include "my_sys.h"                 // strmake_root
#include "mysql/strings/m_ctype.h"  // CHARSET_INFO
#include "sql_lexer_parser_state.h"

class THD {
 public:
  using sql_mode_t = uint64_t;

  MEM_ROOT *mem_root{nullptr};  // Pointer to current memroot

  const CHARSET_INFO *charset() const { return variables.character_set_client; }
  bool convert_string(LEX_STRING * /* to */, const CHARSET_INFO * /* to_cs */,
                      const char * /* from */, size_t /* from_length */,
                      const CHARSET_INFO * /* from_cs */,
                      bool /* report_error */ = false) {
    return true;
  }

  void *alloc(size_t size) { return mem_root->Alloc(size); }

  char *strmake(const char *str, size_t size) const {
    return strmake_root(mem_root, str, size);
  }

  struct System_variables {
    System_variables();

    sql_mode_t sql_mode{};
    const CHARSET_INFO *character_set_client{};
    const CHARSET_INFO *default_collation_for_utf8mb4{};
  };

  System_variables variables;

  Parser_state *m_parser_state{};
};

#endif
