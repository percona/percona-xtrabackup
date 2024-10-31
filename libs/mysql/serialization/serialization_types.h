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

#ifndef MYSQL_SERIALIZATION_SERIALIZATION_TYPES_H
#define MYSQL_SERIALIZATION_SERIALIZATION_TYPES_H

/// @file
/// Experimental API header

#include <cstdint>
#include <string>

/// @addtogroup GroupLibsMysqlSerialization
/// @{

namespace mysql::serialization {

/// @brief Type of the "level" used to indicate on which level of the nested
/// message internal fields are defined
using Level_type = std::size_t;
/// @brief Type for field_id assigned to each field in the
using Field_id_type = uint64_t;
using Field_size = std::size_t;

}  // namespace mysql::serialization

/// @}

#endif  // MYSQL_SERIALIZATION_SERIALIZATION_TYPES_H
