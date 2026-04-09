# ==== Purpose ====
#
# Verify that error messages from failed statements contain the *interpolated*
# query, when that is needed and possible. Interpolated means that the query
# is generated, possibly using $variables. Interpolated queries are used in the
# following mysqltest syntaxes:
#   eval $query;
#   send_eval $query; reap;
#   --let $value = `$query`
#   --let $value = query_get_value($query, column, row)
#
# ==== Requirements ====
#
# R1. When an interpolated query fails, and is not expected to fail, the error
#     message shall contain both the uninterpolated query and the interpolated
#     query.
#
#     Exception: Currently this is not implemented for send_eval.
#
# R2. When an interpolated query fails, but is expected to fail with a different
#     error code, the error message shall contain both the uninterpolated query
#     and the interpolated query.
#
#     Exception: Currently this is not implemented for send_eval.
#
# R3. When an interpolated query succeeds, and is expected to fail, the error
#     message shall contain both the uninterpolated query and the interpolated
#     query.
#
#     Exception: Currently this is not implemented for send_eval.
#
# R4. Messages for non-interpolated queries shall only contain the query once.


# mysqltest has a bug in its interpolation/escaping mechanism:
# - It interprets '$query' by replacing $query by the value of $query, which is
#   OK.
# - Now that we need a string that contains a literal dollar character, we would
#   need a way to escape it.
# - But mysqltest interprets '\$dollar' as '\$dollar', i.e., preserves the
#   backslash, contrary to all normal interpolation logic.
# So it follows that we have to use weird ways like the following to get a
# string containing an unescaped dollar. This makes the variable $query hold the
# text $query, the variable $value hold the text $value.
--let $query = `SELECT CONCAT(CHAR(36), 'query')`
--let $value = `SELECT CONCAT(CHAR(36), 'value')`

# Set the variable $have_windows.
--source include/check_windows.inc

# Execute one scenario
--let $scenario = $MYSQLTEST_VARDIR/tmp/scenario.inc
--write_file $scenario SCENARIO_END
  --echo # $script
  if ($have_windows) {
    --error 1
    --exec echo "$script" | $MYSQL_TEST 2>&1
  }
  if (!$have_windows) {
    --error 1
    --exec echo '$script' | $MYSQL_TEST 2>&1
  }
SCENARIO_END

--echo #### R1. Expect no error, but error observed

--echo # R1.1. Interpolated queries

--let $script = let $query = invalid; eval $query;
--source $scenario
--let $script = let $query = invalid; send_eval $query; reap;
--source $scenario
--let $script = let $query = invalid; let $value = `$query`;
--source $scenario
--let $script = let $query = invalid; let $value = query_get_value($query, x, 1);
--source $scenario
--let $script = let $query = invalid; if (`$query`) { echo true; }
--source $scenario
--let $script = let $query = invalid; while (`$query`) { echo true; }
--source $scenario

--echo # R1.2. Interpolating command, but constant query text

--let $script = eval invalid;
--source $scenario
--let $script = send_eval invalid; reap;
--source $scenario
--let $script = let $value = `invalid`;
--source $scenario
--let $script = let $value = query_get_value(invalid, x, 1);
--source $scenario
--let $script = if (`invalid`) { echo true; }
--source $scenario
--let $script = while (`invalid`) { echo true; }
--source $scenario

--echo #### R2. Expected error, but other error observed

--echo # R2.1. Interpolated queries

--let $script = let $query = invalid; error 4711; eval $query;
--source $scenario
--let $script = let $query = invalid; error 47,11; eval $query;
--source $scenario
--let $script = let $query = invalid; send_eval $query; error 4711; reap;
--source $scenario
--let $script = let $query = invalid; send_eval $query; error 47,11; reap;
--source $scenario

--echo # R2.2. Interpolating command, but constant query text

--let $script = error 4711; eval invalid;
--source $scenario
--let $script = error 47,11; eval invalid;
--source $scenario
--let $script = send_eval invalid; error 4711; reap;
--source $scenario
--let $script = send_eval invalid; error 47,11; reap;
--source $scenario

--echo #### R3. Expected error, but no error observed

--echo # R3.1. Interpolated queries

--let $script = let $query = SELECT 1; error 4711; eval $query;
--source $scenario
--let $script = let $query = SELECT 1; error 47,11; eval $query;
--source $scenario
--let $script = let $query = SELECT 1; send_eval $query; error 4711; reap;
--source $scenario
--let $script = let $query = SELECT 1; send_eval $query; error 47,11; reap;
--source $scenario

--echo # R3.2. Interpolating command, but constant query text

--let $script = error 4711; eval SELECT 1;
--source $scenario
--let $script = error 47,11; eval SELECT 1;
--source $scenario
--let $script = send_eval SELECT 1; error 4711; reap;
--source $scenario
--let $script = send_eval SELECT 1; error 47,11; reap;
--source $scenario

--echo #### R4. Non-interpolated queries

--let $script = invalid;
--source $scenario

--let $script = error 4711; invalid;
--source $scenario
--let $script = error 47,11; invalid;
--source $scenario

--let $script = error 4711; SELECT 1;
--source $scenario
--let $script = error 47,11; SELECT 1;
--source $scenario

--remove_file $scenario
