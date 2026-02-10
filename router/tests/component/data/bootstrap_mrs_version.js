var common_stmts = require("common_statements");

var common_responses = common_stmts.prepare_statement_responses(
    [
      "router_set_session_options",
      "router_set_gr_consistency_level",
      "router_show_cipher_status",
    ],
    {});

({
  stmts: function(stmt) {
    if (common_responses.hasOwnProperty(stmt)) {
      return common_responses[stmt];
    } else if (
        stmt === "SELECT * FROM mysql_innodb_cluster_metadata.schema_version") {
      return {
        error: {
          code: 1049,
          sql_state: "HY001",
          message: "Unknown database mysql_innodb_cluster_metadata"
        }
      };
    } else if (
        stmt ===
        "SELECT substring_index(@@version, '.', 1), concat(@@version_comment, @@version)") {
      var ver = mysqld.global.server_version;
      return {
        result: {
          columns: [
            {name: "substring_index(@@version, '.', 1)", type: "STRING"},
            {name: "concat(@@version_comment, @@version)", type: "STRING"},
          ],
          rows: [
            [ver.substring(0, 1), "MySQL" + ver],
          ]
        }
      };
    } else if (stmt === "SELECT @@basedir") {
      return {
        result: {
          columns: [
            {name: "@@basedir", type: "STRING"},
          ],
          rows: [
            ["tmp"],
          ]
        }
      };
    } else if (stmt === "SELECT aurora_version()") {
      return {
        error: {
          code: 1046,
          sql_state: "HY001",
          message: "No database selected",
        }
      };
    } else if (
        stmt ===
        "SELECT `major`,`minor`,`patch` FROM mysql_rest_service_metadata.schema_version;") {
      var ver = mysqld.global.server_version;
      return {
        result: {
          columns: [
            {name: "major", type: "STRING"},
            {name: "minor", type: "STRING"},
            {name: "patch", type: "STRING"},
          ],
          rows: [
            [3, 0, 0],
          ]
        }
      };
    } else if (stmt === "SELECT @@mysqlx_port") {
      return {
        result: {
          columns: [
            {name: "@@mysqlx_port", type: "STRING"},
          ],
          rows: [
            ["33060"],
          ]
        }
      };
    } else {
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
