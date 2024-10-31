// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#ifndef MYSQL_UTILS_DEPRECATE_HEADER_H
#define MYSQL_UTILS_DEPRECATE_HEADER_H

/// @file
/// Experimental API header
/// Contains macro that can be used to deprecate a header

/// @addtogroup GroupLibsMysqlUtils
/// @{

#define DEPRECATE_HEADER(header_name)                              \
  namespace {                                                      \
  [[deprecated("This header is deprecated")]] constexpr static int \
      header_name##_header_deprecation = 0;                        \
  constexpr static int header_name##_header_is_deprecated =        \
      header_name##_header_deprecation;                            \
  }  // namespace

/// @}

#endif  // MYSQL_UTILS_DEPRECATE_HEADER_H
