var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

if (mysqld.global.gr_node_host === undefined) {
  mysqld.global.gr_node_host = "127.0.0.1";
}

if (mysqld.global.gr_id === undefined) {
  mysqld.global.gr_id = "uuid";
}

if (mysqld.global.gr_nodes === undefined) {
  mysqld.global.gr_nodes = [];
}

if (mysqld.global.cluster_nodes === undefined) {
  mysqld.global.cluster_nodes = [];
}

if (mysqld.global.notices === undefined) {
  mysqld.global.notices = [];
}

if (mysqld.global.md_query_count === undefined) {
  mysqld.global.md_query_count = 0;
}

if (mysqld.global.transaction_count === undefined) {
  mysqld.global.transaction_count = 0;
}

if (mysqld.global.cluster_name === undefined) {
  mysqld.global.cluster_name = "test";
}

if (mysqld.global.mrs_router_id === undefined) {
  mysqld.global.mrs_router_id = 1;
}

({
  handshake: {
    auth: {
      username: mysqld.global.user,
      password: mysqld.global.password,
    },
    greeting: {server_version: mysqld.global.server_version}
  },
  stmts: function(stmt) {
    // ensure the cluster-type is set even if set_mock_metadata() did not set
    // it.
    if (mysqld.global.cluster_type === undefined) {
      mysqld.global.cluster_type = "gr";
    }

    var members = gr_memberships.gr_members(
        mysqld.global.gr_node_host, mysqld.global.gr_nodes);

    var options = {
      group_replication_members: members,
      innodb_cluster_instances: gr_memberships.cluster_nodes(
          mysqld.global.gr_node_host, mysqld.global.cluster_nodes),
      gr_id: mysqld.global.gr_id,
      innodb_cluster_name: mysqld.global.cluster_name,
      mrs_router_id: mysqld.global.mrs_router_id,
    };

    // prepare the responses for common statements
    var common_responses = common_stmts.prepare_statement_responses(
        [
          "router_set_session_options",
          "router_set_gr_consistency_level",
          "router_select_cluster_type_v2",
          "select_port",
          "router_commit",
          "router_rollback",
          "router_select_schema_version",
          "router_check_member_state",
          "router_select_members_count",
          "router_select_group_membership",
          "router_clusterset_present",
          "router_select_router_options_view",
          "get_routing_guidelines_version",
          "get_guidelines_router_info",
          "get_routing_guidelines",
          "mrs_set_sql_mode",
          "mrs_set_meta_provider_role",
          "mrs_select_version",
          "mrs_select_basedir",
          "mrs_set_data_provider_role",


        ],
        options);

    var common_responses_regex = common_stmts.prepare_statement_responses_regex(
        [
          "mrs_select_router_id",
        ],
        options);

    var router_select_metadata =
        common_stmts.get("router_select_metadata_v2_gr", options);

    var router_start_transaction =
        common_stmts.get("router_start_transaction", options);


    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt === "SELECT aurora_version()") {
      return {
        error: {
          code: 1046,
          sql_state: "HY001",
          message: "No database selected",
        }
      };
    } else if (stmt === router_start_transaction.stmt) {
      mysqld.global.transaction_count++;
      return router_start_transaction;
    } else if (stmt === router_select_metadata.stmt) {
      mysqld.global.md_query_count++;
      return router_select_metadata;
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  },
  notices: (function() {
    return mysqld.global.notices;
  })()
})
