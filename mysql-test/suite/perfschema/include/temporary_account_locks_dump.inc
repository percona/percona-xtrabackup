# Helper for hostcache_*.test

--echo "Dumping performance_schema.temporary_account_locks"

--disable_query_log
--vertical_results
select 
  USER, HOST,
  LOCKED,
  FAILED_LOGIN_ATTEMPTS,
  REMAINING_LOGIN_ATTEMPTS,
  PASSWORD_LOCK_TIME,
  if (LOCKED_SINCE is not null,
     if (LOCKED_SINCE > date("2025-09-01"), "set", "wrong epoch"),
     "null") as LOCKED_SINCE,
  if (LOCKED_UNTIL is not null,
     if (LOCKED_UNTIL > date("2025-09-01"), "set", "wrong epoch"),
     "null") as LOCKED_UNTIL
  from performance_schema.temporary_account_locks;
--horizontal_results
--enable_query_log

