--source include/not_hypergraph.inc

--source include/force_myisam_default.inc
--source include/have_myisam.inc

# This test doesn't support GTID
--source include/rpl/set_gtid_mode_off.inc

let $engine=myisam;
--source include/skip_scan_test.inc

# Restore the default for gtid_mode which is ON
--source include/rpl/set_gtid_mode_on.inc
