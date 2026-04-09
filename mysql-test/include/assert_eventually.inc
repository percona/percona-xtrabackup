# ==== Purpose ====
#
# Wait until a condition holds, fail with debug info if it does not become
# true within defined timeout period (defaulting to 5 minutes).
#
# The condition has the same form as expressions evaluated by include/eval.inc
#
# This is similar to wait_condition.inc, with the following differences:
#
# - This file is meant to be used to verify a test requirement, whereas
#   wait_contion.inc is meant to be used to prepare a test scenario.
#   So, both scripts wait for the server to reach a given state;
#   wait_condition.inc should be used when that state is just needed to avoid
#   a race condition in the test; assert_eventually.inc should be used when
#   it is the purpose of the test to verify that the state is reached.
#
# - This file prints to the result file; wait_condition.inc does not.
#
# - This file uses eval.inc to evaluate compound expressions; wait_condition.inc
#   does not.
#
# ==== Usage ====
#
# --let $assert_text = Relay_Log_Pos must be between min_pos and max_pos
# --let $assert_cond = [SHOW REPLICA STATUS, Relay_Log_Pos, 1] >= $min_pos AND <1> <= $max_pos
# [--let $assert_timeout = N]
# [--let $extra_debug_info = some text]
# [--let $extra_debug_eval = expression parsable by include/eval.inc]
# [--let $rpl_debug = 1]
# --source include/assert_eventually.inc
#
# Parameters:
#
# $assert_text, $assert_cond, $rpl_debug, $assert_no_stop,
# $extra_debug_info, $extra_debug_eval
#   See assert.inc.
#   Note: contrary to assert.inc, this script does not have an $assert_escape
#   parameter. This script always escapes properly.
#
# $assert_timeout
#   If the condition does not hold within this number of seconds, fail the test.
#   If this variable is not set, the timeout defaults to 5 minutes.


--let $include_filename = assert_eventually.inc [$assert_text]
--source include/begin_include_file.inc

if ($rpl_debug)
{
  --echo # debug: assert_text='$assert_text' assert_cond='$assert_cond'
}

# Sanity-check input
if (!$assert_text)
{
  --die ERROR IN TEST: the mysqltest variable assert_text must be set
}

--let $_assert_eventually_timeout_tenths = 3000
if ($assert_timeout != '') {
  --let $ten = 10
  --expr $_assert_eventually_timeout_tenths = $assert_timeout * $ten
}
--let $_assert_eventually_waited_tenths = 0
--let $_assert_eventually_sleep_tenths = 1
--let $_assert_eventually_sleep_seconds = 0.1

--let $_assert_eventually_old_eval_result = $eval_result

--let $_assert_eventually_old_eval_expr = $eval_expr
--let $_assert_eventually_old_eval_no_result = $eval_no_result
--let $_assert_eventually_old_eval_escape = $eval_escape

--let $eval_expr = $assert_cond
--let $eval_escape = 1
--let $eval_result = 0

--let $done = 0
while (!$done)
{
  # Check.
  --source include/eval.inc

  if ($eval_result) {
    --let $done = 1
  }
  --expr $_assert_eventually_waited_tenths = $_assert_eventually_waited_tenths + $_assert_eventually_sleep_tenths
  # After 10 seconds, just check once per second instead of 10 times per second.
  if ($_assert_eventually_waited_tenths == 100) {
    --let $_assert_eventually_sleep_tenths = 10
    --let $_assert_eventually_sleep_seconds = 1
  }
  if ($_assert_eventually_waited_tenths >= $_assert_eventually_timeout_tenths) {
    --let $done = 1
  }
  if (!$done) {
    --sleep $_assert_eventually_sleep_seconds
  }
}

--let $eval_escape = $_assert_eventually_old_eval_escape
--let $eval_expr = $_assert_eventually_old_eval_expr
--let $eval_no_result = $_assert_eventually_old_eval_no_result

if (!$eval_result) {
  # We reached here after the timeout, without eval.inc returning success.
  # Do a normal assertion which will (likely) fail and print debug info.
  --source include/assert.inc
}

--let $include_filename = assert_eventually.inc [$assert_text]
--source include/end_include_file.inc

--let $eval_result = $_assert_eventually_old_eval_result
--let $assert_text =
--let $assert_cond =
