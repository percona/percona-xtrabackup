
var common_stmts = require("common_statements");
var gr_memberships = require("gr_memberships");

if (mysqld.global.gr_node_host === undefined) {
  mysqld.global.gr_node_host = "127.0.0.1";
}

if (mysqld.global.routing_guidelines === undefined) {
  mysqld.global.routing_guidelines = "";
}

if (mysqld.global.router_rw_classic_port === undefined) {
  mysqld.global.router_rw_classic_port = "";
}

if (mysqld.global.router_ro_classic_port === undefined) {
  mysqld.global.router_ro_classic_port = "";
}

if (mysqld.global.router_rw_x_port === undefined) {
  mysqld.global.router_rw_x_port = "";
}

if (mysqld.global.router_ro_x_port === undefined) {
  mysqld.global.router_ro_x_port = "";
}

if (mysqld.global.router_rw_split_classic_port === undefined) {
  mysqld.global.router_rw_split_classic_port = "";
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

if (mysqld.global.transaction_count === undefined) {
  mysqld.global.transaction_count = 0;
}

if (mysqld.global.cluster_type === undefined) {
  mysqld.global.cluster_type = "gr";
}

if (mysqld.global.cluster_name === undefined) {
  mysqld.global.cluster_name = "test";
}

if (mysqld.global.router_options === undefined) {
  mysqld.global.router_options = "{}";
}

if (mysqld.global.metadata_schema_version === undefined) {
  mysqld.global.metadata_schema_version = [2, 3, 0];
}

var events = {};

// increment the event counter.
//
// if it doesn't exist yet, pretend it is zero
function increment_event(event_name) {
  events[event_name] = (events[event_name] || 0) + 1;
}

({
  handshake: {
    auth: {
      username: mysqld.global.user,
      password: mysqld.global.password,
    }
  },
  stmts: function(stmt) {
    var members = gr_memberships.gr_members(
        mysqld.global.gr_node_host, mysqld.global.gr_nodes);

    const member_state = members[mysqld.global.gr_pos] ?
        members[mysqld.global.gr_pos][3] :
        undefined;

    var options = {
      group_replication_members: members,
      gr_member_state: member_state,
      innodb_cluster_instances: gr_memberships.cluster_nodes(
          mysqld.global.gr_node_host, mysqld.global.cluster_nodes),
      gr_id: mysqld.global.gr_id,
      cluster_type: mysqld.global.cluster_type,
      innodb_cluster_name: mysqld.global.cluster_name,
      router_options: mysqld.global.router_options,
      metadata_schema_version: mysqld.global.metadata_schema_version,
      routing_guidelines: mysqld.global.routing_guidelines,
      router_info: mysqld.global.router_info,
      router_rw_classic_port: mysqld.global.router_rw_classic_port,
      router_ro_classic_port: mysqld.global.router_ro_classic_port,
      router_rw_split_classic_port: mysqld.global.router_rw_split_classic_port,
      router_rw_x_port: mysqld.global.router_rw_x_port,
      router_ro_x_port: mysqld.global.router_ro_x_port,
    };

    // prepare the responses for common statements
    var common_responses = common_stmts.prepare_statement_responses(
        [
          "router_set_session_options",
          "router_set_gr_consistency_level",
          "router_select_cluster_type_v2",
          "router_commit",
          "router_rollback",
          "router_select_schema_version",
          "router_check_member_state",
          "router_select_members_count",
          "router_select_group_membership",
          "router_clusterset_present",
          "router_select_router_options_view",
          "router_update_last_check_in_v2_4",
          "get_routing_guidelines_version",
          "get_guidelines_router_info",
          "get_routing_guidelines",
        ],
        options);

    var common_responses_regex = common_stmts.prepare_statement_responses_regex(
        [
          "router_update_attributes_v2",
        ],
        options);

    var select_port = common_stmts.get("select_port");

    var router_select_metadata =
        common_stmts.get("router_select_metadata_v2_gr", options);

    var router_start_transaction =
        common_stmts.get("router_start_transaction", options);

    var statement_sql_set_option = "statement/sql/set_option";
    var statement_sql_select = "statement/sql/select";
    var statement_unknown_command = "command/unknown";

    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        (res = common_stmts.handle_regex_stmt(stmt, common_responses_regex)) !==
        undefined) {
      return res;
    } else if (stmt === router_start_transaction.stmt) {
      mysqld.global.transaction_count++;
      return router_start_transaction;
    } else if (stmt === router_select_metadata.stmt) {
      mysqld.global.md_query_count++;
      return router_select_metadata;
    } else if (stmt === select_port.stmt) {
      increment_event(statement_sql_select);

      return select_port;
    } else if (
        stmt ===
        "SELECT 'collation_connection', @@SESSION.`collation_connection` UNION SELECT 'character_set_client', @@SESSION.`character_set_client` UNION SELECT 'sql_mode', @@SESSION.`sql_mode`") {
      increment_event(statement_sql_select);

      return {
        result: {
          columns: [
            {name: "collation_connection", type: "STRING"},
            {name: "@@SESSION.collation_connection", type: "STRING"},
          ],
          rows: [
            ["collation_connection", "utf8mb4_0900_ai_ci"],
            ["character_set_client", "utf8mb4"],
            ["sql_mode", "bar"],
          ]
        }
      };
    } else if (
        stmt ===
        "SELECT ATTR_NAME, ATTR_VALUE FROM performance_schema.session_account_connect_attrs WHERE PROCESSLIST_ID = CONNECTION_ID() ORDER BY ATTR_NAME") {
      increment_event(statement_sql_select);

      return {
        result: {
          columns: [
            {name: "ATTR_NAME", type: "STRING"},
            {name: "ATTR_VALUE", type: "STRING"},
          ],
          rows: [
            ["foo", "bar"],
          ]
        }
      };
    } else if (
        stmt ===
        "SELECT EVENT_NAME, COUNT_STAR FROM performance_schema.events_statements_summary_by_thread_by_event_name AS e JOIN performance_schema.threads AS t ON (e.THREAD_ID = t.THREAD_ID) WHERE t.PROCESSLIST_ID = CONNECTION_ID() AND COUNT_STAR > 0 ORDER BY EVENT_NAME") {
      var rows = Object.keys(events)
                     .filter(function(key) {
                       // COUNT_START > 0
                       return events[key] > 0;
                     })
                     .sort()  // ORDER BY event_name
                     .reduce(function(collector, key) {
                       var value = events[key];

                       collector.push([key, value]);

                       return collector;
                     }, []);

      increment_event(statement_sql_select);

      return {
        result: {
          columns: [
            {name: "EVENT_NAME", type: "STRING"},
            {name: "COUNT_START", type: "LONG"},
          ],
          rows: rows,
        }
      };
    } else if (
        stmt.indexOf('SET @@SESSION.session_track_system_variables = "*"') !==
        -1) {
      increment_event(statement_sql_set_option);

      // the trackers that are needed for connection-sharing.
      return {
        ok: {
          session_trackers: [
            {
              type: "system_variable",
              name: "session_track_system_variables",
              value: "*"
            },
            {
              type: "system_variable",
              name: "session_track_gtids",
              value: "OWN_GTID"
            },
            {
              type: "system_variable",
              name: "session_track_transaction_info",
              value: "CHARACTERISTICS"
            },
            {
              type: "system_variable",
              name: "session_track_state_change",
              value: "ON"
            },
            {
              type: "system_variable",
              name: "session_track_schema",
              value: "ON"
            },
            {
              type: "trx_characteristics",
              value: "",
            },
          ]
        }
      };
    } else {
      increment_event(statement_unknown_command);

      return {
        error: {
          code: 1273,
          sql_state: "HY001",
          message: "Syntax Error at: " + stmt
        }
      };
    }
  }
})
