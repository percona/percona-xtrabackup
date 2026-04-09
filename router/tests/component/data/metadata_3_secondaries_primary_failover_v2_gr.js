/**
 * run 4 nodes on the current host
 *
 * - 1 PRIMARY
 * - 3 SECONDARY
 *
 * via HTTP interface
 *
 * - primary_failover
 */

var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

var gr_node_host = "127.0.0.1";

if (mysqld.global.cluster_name === undefined) {
  mysqld.global.cluster_name = "test";
}

if (mysqld.global.routing_guidelines === undefined) {
  mysqld.global.routing_guidelines = "";
}

// at start, .connects is undefined
// at first connect, set it to 0
// at each following connect, increment it.
//
// .globals is shared between mock-server threads
if (mysqld.global.connects === undefined) {
  mysqld.global.connects = 0;
} else {
  mysqld.global.connects = mysqld.global.connects + 1;
}

({
  stmts: function(stmt) {
    var nodes = function(host, port_and_state) {
      return port_and_state.map(function(current_value) {
        return [
          current_value[0], host, current_value[1], current_value[2],
          current_value[3]
        ];
      });
    };
    var options = {
      group_replication_members: nodes(gr_node_host, mysqld.global.gr_nodes),
      innodb_cluster_instances: gr_memberships.cluster_nodes(
          mysqld.global.gr_node_host, mysqld.global.cluster_nodes),
      cluster_type: "gr",
      gr_id: mysqld.global.gr_id,
      router_rw_classic_port: mysqld.global.router_rw_classic_port,
      router_ro_classic_port: mysqld.global.router_ro_classic_port,
      router_rw_split_classic_port: mysqld.global.router_rw_split_classic_port,
      router_rw_x_port: mysqld.global.router_rw_x_port,
      router_ro_x_port: mysqld.global.router_ro_x_port,
      router_rw_split_classic_port: mysqld.global.router_rw_split_classic_port,
      routing_guidelines: mysqld.global.routing_guidelines,
      router_info: mysqld.global.router_info,
      innodb_cluster_name: mysqld.global.cluster_name,
    };

    var options_failover = {
      group_replication_members: nodes(gr_node_host, mysqld.global.gr_nodes),
      innodb_cluster_instances: gr_memberships.cluster_nodes(
          mysqld.global.gr_node_host, mysqld.global.cluster_nodes),
      cluster_type: "gr",
      router_rw_classic_port: mysqld.global.router_rw_classic_port,
      router_ro_classic_port: mysqld.global.router_ro_classic_port,
      router_rw_split_classic_port: mysqld.global.router_rw_split_classic_port,
      router_rw_x_port: mysqld.global.router_rw_x_port,
      router_ro_x_port: mysqld.global.router_ro_x_port,
      router_rw_split_classic_port: mysqld.global.router_rw_split_classic_port,
      routing_guidelines: mysqld.global.routing_guidelines,
      router_info: mysqld.global.router_info,
    };
    // in the startup case, first node is PRIMARY
    // in case of failover, announce the 2nd node as PRIMARY
    options_failover.group_replication_members[0][4] = "SECONDARY";
    options_failover.group_replication_members[1][4] = "PRIMARY";

    if (mysqld.global.transaction_count === undefined) {
      mysqld.global.transaction_count = 0;
    }

    // prepare the responses for common statements
    var common_responses = common_stmts.prepare_statement_responses(
        [
          "router_set_session_options",
          "router_set_gr_consistency_level",
          "select_port",
          "router_commit",
          "router_select_schema_version",
          "router_select_cluster_type_v2",
          "router_select_metadata_v2_gr",
          "router_clusterset_present",
          "router_check_member_state",
          "router_select_members_count",
          "router_select_router_options_view",
          "get_guidelines_router_info",
          "get_routing_guidelines",
          "router_update_last_check_in_v2_4",
        ],
        options);

    var common_responses_regex = common_stmts.prepare_statement_responses_regex(
        [
          "router_update_routers_in_metadata",
          "router_update_attributes_v2",
        ],
        options);

    if (mysqld.global.primary_failover === undefined) {
      mysqld.global.primary_failover = false;
    }

    // allow to switch
    var router_select_group_membership =
        common_stmts.get("router_select_group_membership", options);
    var router_select_group_membership_failover =
        common_stmts.get("router_select_group_membership", options_failover);

    var router_start_transaction =
        common_stmts.get("router_start_transaction", options);

    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt === router_start_transaction.stmt) {
      mysqld.global.transaction_count++;
      return router_start_transaction;
    } else if (stmt === router_select_group_membership.stmt) {
      if (!mysqld.global.primary_failover) {
        return router_select_group_membership;
      } else {
        return router_select_group_membership_failover;
      }
    } else {
      return common_stmts.unknown_statement_response(stmt);
    }
  }
})
