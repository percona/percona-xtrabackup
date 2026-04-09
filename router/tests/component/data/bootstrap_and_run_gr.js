var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

if (mysqld.global.cluster_name == undefined) {
  mysqld.global.cluster_name = "my-cluster";
}

if (mysqld.global.metadata_schema_version === undefined) {
  mysqld.global.metadata_schema_version = [2, 2, 0];
}

if (mysqld.global.gr_id === undefined) {
  mysqld.global.gr_id = "cluster-specific-id";
}

var gr_node_host = "127.0.0.1";

var options = {
  metadata_schema_version: mysqld.global.metadata_schema_version,
  cluster_type: "gr",
  gr_id: mysqld.global.gr_id,
  clusterset_present: 0,
  innodb_cluster_name: mysqld.global.cluster_name,
  innodb_cluster_instances: gr_memberships.cluster_nodes(
      mysqld.global.gr_node_host, mysqld.global.cluster_nodes),
  router_version: mysqld.global.router_version,
  group_replication_members:
      gr_memberships.gr_members(gr_node_host, mysqld.global.gr_nodes),
};

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_select_schema_version",
      "router_select_cluster_type_v2",
      "router_select_current_instance_attributes",
      "router_count_clusters_v2",
      "router_check_member_state",
      "router_select_members_count",
      "router_select_replication_group_name",
      "router_show_cipher_status",
      "router_select_cluster_instances_v2_gr",
      "router_start_transaction",
      "router_commit",
      "get_routing_guidelines_version",
      "get_guidelines_router_info",
      "get_routing_guidelines",
      "router_select_router_options_view",
      "get_routing_guidelines_version",
      "get_guidelines_router_info",
      "router_select_metadata_v2_gr",
      "router_update_attributes_v2",
      "router_update_last_check_in_v2_4",
      "get_local_cluster_name",
      "router_select_group_membership",
      "router_select_metadata_v2_gr_account_verification",
      "router_clusterset_present",
    ],
    options);

var common_responses_regex = common_stmts.prepare_statement_responses_regex(
    [
      "router_insert_into_routers", "router_create_user_if_not_exists",
      "router_grant_on_metadata_db", "router_grant_on_pfs_db",
      "router_grant_on_routers", "router_grant_on_v2_routers",
      "router_grant_on_router_stats", "router_update_routers_in_metadata",
      "router_update_router_options_in_metadata", "router_update_attributes_v2",
      "router_update_local_cluster_in_metadata",
      "router_select_config_defaults_stored_gr_cluster"
    ],
    options);

({
  handshake: {
    auth: {
      username: mysqld.global.user,
      password: mysqld.global.password,
    }
  },
  stmts: function(stmt) {
    var res;
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
