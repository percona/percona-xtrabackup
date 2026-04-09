# ==== Purpose ====
#
# Wait until the number of prepared XA transactions equals the given number.
# The number of prepared XA transactions is given by the size of the result set
# for XA RECOVER.
#
# Note: When the server executes XA PREPARE, it externalizes the XID after the
# GTID. Therefore, a test that uses e.g. sync_to_replica.inc to wait until the
# replica has applied an XA PREPARE transaction, may not immediately observe the
# XID. So, for example, a subsequent XA COMMIT on the replica may fail because
# it does not find the XID. To solve this problem use this script to wait for
# the XID to be externalized.
#
# ==== Usage ====
#
# --let $xa_prepared_count = INTEGER
# [--let $slave_timeout = SECONDS]
# --source include/wait_for_xa_prepared_count.inc
#
# $xa_prepared_count
#   The expected line count return by `XA RECOVER`
#
# $slave_timeout
#   The number of seconds to wait until failing with timeout. Defaults to 300.
#   We use this name in order to be compatible with include/rpl/wait_for_*.inc,
#   so that users that need to set a timeout for any wait in their test can use
#   a single variable.

--let $include_filename = wait_for_xa_prepared_count.inc
--source include/begin_include_file.inc

--let $_wfxpc_timeout = $slave_timeout
if (!$_wfxpc_timeout) {
  --let $_wfxpc_timeout = 300
}
--let $ten = 10
--expr $_wfxpc_remaining_iterations = $_wfxpc_timeout * $ten

while ($_wfxpc_remaining_iterations) {
  --let $statement = XA RECOVER
  --let $column = data
  --source include/get_row_count.inc
  if ($row_count == $xa_prepared_count) {
    --let $_wfxpc_remaining_iterations = 0
  }
  if ($row_count != $xa_prepared_count) {
    --sleep 0.1
    --dec $_wfxpc_remaining_iterations
    if ($_wfxpc_remaining_iterations == 0) {
      --source include/rpl/debug/show_debug_info.inc
      --echo Timeout after $_wfxpc_timeout seconds in wait_for_xa_prepared_count.inc
      --echo Expected $xa_prepared_count prepared transactions, but this is what we have:
      XA RECOVER;
    }
  }
}

--let $include_filename = wait_for_xa_prepared_count.inc
--source include/end_include_file.inc
