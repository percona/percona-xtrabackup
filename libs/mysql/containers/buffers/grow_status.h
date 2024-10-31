/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

/// @file

#ifndef MYSQL_CONTAINERS_BUFFERS_GROW_STATUS_H
#define MYSQL_CONTAINERS_BUFFERS_GROW_STATUS_H

#include <ostream>
#include <string>

/// @addtogroup GroupLibsMysqlContainers
/// @{

namespace mysql::containers::buffers {

/// Error statuses for classes that use Grow_calculator.
enum class Grow_status {
  /// A grow operation succeeded. The data structure is now of the new
  /// size.
  success,
  /// A grow operation could not be performed because there is a
  /// configured maximum size. The data structure is unchanged.
  exceeds_max_size,
  /// A grow operation failed because memory allocation failed. The
  /// data structure is unchanged.
  out_of_memory
};

/// Return value from debug_string(Grow_status) when the parameter is
/// not a valid value.
extern const std::string invalid_grow_status_string;

/// Return a string that describes each enumeration value.
std::string debug_string(Grow_status status);

/// Write a string that describes the enumeration value to the stream.
std::ostream &operator<<(std::ostream &stream, Grow_status status);

}  // namespace mysql::containers::buffers

/// @}

#endif /* MYSQL_CONTAINERS_BUFFERS_GROW_STATUS_H */
