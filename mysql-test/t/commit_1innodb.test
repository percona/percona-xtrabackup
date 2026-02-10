--source include/not_hypergraph.inc  # Does not use ha_delete_all_rows().
-- source include/rpl/force_binlog_format_statement.inc
# This test doesn't support GTID
--source include/rpl/set_gtid_mode_off.inc

call mtr.add_suppression("Unsafe statement written to the binary log using statement format since BINLOG_FORMAT = STATEMENT");

let $engine_type = InnoDB;

-- source include/commit.inc

--source include/rpl/restore_default_binlog_format.inc
# Restore the default for gtid_mode which is ON
--source include/rpl/set_gtid_mode_on.inc
