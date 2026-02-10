--source include/force_myisam_default.inc
--source include/have_myisam.inc

# This test doesn't support GTID
--source include/rpl/set_gtid_mode_off.inc

let $engine= MyISAM;
--source include/ctype_utf8mb4.inc

# Restore the default for gtid_mode which is ON
--source include/rpl/set_gtid_mode_on.inc
