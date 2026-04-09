--source include/force_myisam_default.inc
--source include/have_myisam.inc
# This test doesn't support GTID
--source include/rpl/set_gtid_mode_off.inc

--echo #
--echo # Function defaults run 1. No microsecond precision. MyISAM.
--echo #
set default_storage_engine=myisam;
let $current_timestamp=CURRENT_TIMESTAMP;
let $now=NOW();
let $timestamp=TIMESTAMP;
let $datetime=DATETIME;
source 'include/function_defaults.inc';

# Restore the default for gtid_mode which is ON
--source include/rpl/set_gtid_mode_on.inc
