/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_CONNECTION_POOL_SUPPORTED_OPTIONS_INCLUDED
#define MYSQLROUTER_CONNECTION_POOL_SUPPORTED_OPTIONS_INCLUDED

#include <array>

namespace connection_pool {
namespace options {
constexpr const char kIdleTimeout[]{"idle_timeout"};
constexpr const char kMaxIdleServerConnections[]{"max_idle_server_connections"};
}  // namespace options
}  // namespace connection_pool

static constexpr std::array connection_pool_supported_options{
    connection_pool::options::kIdleTimeout,
    connection_pool::options::kMaxIdleServerConnections,
};

#endif /* MYSQLROUTER_ROUTING_SUPPORTED_ROUTING_INCLUDED */
