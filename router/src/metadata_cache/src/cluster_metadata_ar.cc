/*
  Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "cluster_metadata_ar.h"

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/mysql_session.h"
#include "mysqlrouter/utils.h"
#include "mysqlrouter/utils_sqlstring.h"

using mysqlrouter::MySQLSession;
using mysqlrouter::sqlstring;
using mysqlrouter::strtoi_checked;
using mysqlrouter::strtoui_checked;
IMPORT_LOG_FUNCTIONS()

ARClusterMetadata::~ARClusterMetadata() {}

ClusterMetadata::ReplicaSetsByName ARClusterMetadata::fetch_instances(
    const std::vector<metadata_cache::ManagedInstance> &instances,
    const std::string &cluster_type_specific_id, std::size_t &instance_id) {
  std::vector<metadata_cache::ManagedInstance> new_instances;

  bool metadata_read = false;

  for (size_t i = 0; i < instances.size(); ++i) {
    const auto &instance = instances[i];
    try {
      if (!connect_and_setup_session(instance)) {
        log_warning("Could not connect to the instance: %s on %s:%d",
                    instance.mysql_server_uuid.c_str(), instance.host.c_str(),
                    instance.port);
        continue;
      }

      MySQLSession::Transaction transaction(metadata_connection_.get());

      // throws metadata_cache::metadata_error and
      // MetadataUpgradeInProgressException
      const auto version =
          get_and_check_metadata_schema_version(*metadata_connection_);

      const auto cluster_type =
          mysqlrouter::get_cluster_type(version, metadata_connection_.get());

      if (cluster_type != mysqlrouter::ClusterType::RS_V2) {
        log_error(
            "Invalid cluster type '%s'. Configured '%s'",
            mysqlrouter::to_string(cluster_type).c_str(),
            mysqlrouter::to_string(mysqlrouter::ClusterType::RS_V2).c_str());
        continue;
      }

      unsigned view_id{0};
      if (!get_member_view_id(*metadata_connection_, cluster_type_specific_id,
                              view_id)) {
        log_warning("Failed fetching view_id from the instance: %s",
                    instance.mysql_server_uuid.c_str());
        continue;
      }

      if (view_id < this->view_id_) {
        continue;
      }

      if (view_id == this->view_id_ && metadata_read) {
        continue;
      }

      new_instances = fetch_instances_from_member(*metadata_connection_,
                                                  cluster_type_specific_id);

      this->view_id_ = view_id;
      metadata_read = true;
      instance_id = i;
    } catch (const mysqlrouter::MetadataUpgradeInProgressException &) {
      throw;
    } catch (const std::exception &e) {
      log_warning("Failed fetching metadata from instance: %s on %s:%d - %s",
                  instance.mysql_server_uuid.c_str(), instance.host.c_str(),
                  instance.port, e.what());
    }
  }

  if (new_instances.empty()) {
    return {};
  }

  // pack the result into ManagedReplicaSet to satisfy the API
  metadata_cache::ManagedReplicaSet replicaset;
  ClusterMetadata::ReplicaSetsByName result;

  replicaset.name = "default";
  replicaset.single_primary_mode = true;
  replicaset.members = std::move(new_instances);
  replicaset.view_id = this->view_id_;

  result["default"] = replicaset;

  return result;
}

bool ARClusterMetadata::get_member_view_id(mysqlrouter::MySQLSession &session,
                                           const std::string &cluster_id,
                                           unsigned &result) {
  std::string query =
      "select view_id from mysql_innodb_cluster_metadata.v2_ar_members where "
      "member_id = @@server_uuid";

  if (!cluster_id.empty()) {
    query += " and cluster_id = " + session.quote(cluster_id);
  }

  std::unique_ptr<MySQLSession::ResultRow> row(session.query_one(query));
  if (!row) {
    return false;
  }

  result = strtoui_checked((*row)[0]);

  return true;
}

// throws metadata_cache::metadata_error
std::vector<metadata_cache::ManagedInstance>
ARClusterMetadata::fetch_instances_from_member(
    mysqlrouter::MySQLSession &session, const std::string &cluster_id) {
  std::vector<metadata_cache::ManagedInstance> result;

  // Get expected topology (what was configured) from metadata server. This will
  // later be compared against current topology (what exists NOW) obtained by
  // comparing to other members view of the world
  std::string query =
      "select M.member_id, I.endpoint, I.xendpoint, M.member_role from "
      "mysql_innodb_cluster_metadata.v2_ar_members M join "
      "mysql_innodb_cluster_metadata.v2_instances I on I.instance_id = "
      "M.instance_id join mysql_innodb_cluster_metadata.v2_ar_clusters C on "
      "I.cluster_id = C.cluster_id";

  if (!cluster_id.empty()) {
    query += " where C.cluster_id = " + session.quote(cluster_id);
  }

  // example response
  // clang-format off
  // +--------------------------------------+----------------+-----------------+-------------+
  // | member_id                            | endpoint       | xendpoint       | member_role |
  // +--------------------------------------+----------------+-----------------+-------------+
  // | dc46223b-d620-11e9-9f25-0800276c00e7 | 127.0.0.1:5000 | 127.0.0.1:50000 | PRIMARY     |
  // | f167ceb4-d620-11e9-9155-0800276c00e7 | 127.0.0.1:5001 | 127.0.0.1:50010 | SECONDARY   |
  // | 06c519c9-d621-11e9-9451-0800276c00e7 | 127.0.0.1:5002 | 127.0.0.1:50020 | SECONDARY   |
  // +--------------------------------------+----------------+-----------------+-------------+
  // clang-format on

  auto result_processor = [&result](const MySQLSession::Row &row) -> bool {
    if (row.size() != 4) {
      throw metadata_cache::metadata_error(
          "Unexpected number of fields in the resultset. "
          "Expected = 4, got = " +
          std::to_string(row.size()));
    }

    metadata_cache::ManagedInstance instance;
    instance.mysql_server_uuid = get_string(row[0]);

    if (!set_instance_ports(instance, row, 1, 2)) {
      return true;  // next row
    }

    instance.mode = get_string(row[3]) == "PRIMARY"
                        ? metadata_cache::ServerMode::ReadWrite
                        : metadata_cache::ServerMode::ReadOnly;

    // remainig fields are for compatibility with existing interface so we go
    // with defaults
    instance.replicaset_name = "default";
    instance.role = "HA";
    instance.weight = 0;
    instance.version_token = 0;

    result.push_back(instance);
    return true;  // get next row if available
  };

  assert(session.is_connected());

  try {
    session.query(query, result_processor);
  } catch (const MySQLSession::Error &e) {
    throw metadata_cache::metadata_error(e.what());
  }

  return result;
}
