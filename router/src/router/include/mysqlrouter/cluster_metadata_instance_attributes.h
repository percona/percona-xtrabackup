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

#ifndef MYSQLROUTER_CLUSTER_METADATA_INSTANCE_ATTRIBUTES_INCLUDED
#define MYSQLROUTER_CLUSTER_METADATA_INSTANCE_ATTRIBUTES_INCLUDED

#include "mysqlrouter/router_cluster_export.h"

#include <string>

#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/cluster_metadata.h"

namespace mysqlrouter {

struct InstanceAttributes {
  static stdx::expected<InstanceType, std::string> ROUTER_CLUSTER_EXPORT
  get_instance_type(const std::string &attributes,
                    const mysqlrouter::InstanceType default_instance_type);

  static stdx::expected<bool, std::string> ROUTER_CLUSTER_EXPORT
  get_hidden(const std::string &attributes, bool default_res);

  static stdx::expected<bool, std::string> ROUTER_CLUSTER_EXPORT
  get_disconnect_existing_sessions_when_hidden(const std::string &attributes,
                                               bool default_res);
};

}  // namespace mysqlrouter
#endif  // MYSQLROUTER_CLUSTER_METADATA_INSTANCE_ATTRIBUTES_INCLUDED
