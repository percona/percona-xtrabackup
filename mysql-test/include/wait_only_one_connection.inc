# Select content from given performance schema table (always).
# and database table (if given).
#
# Usage
#
# [--let $wait_timeout= <seconds>]
# --source include/count_sessions.inc
#
# Parameters:
#   $wait_timeout
#     wait timeout in seconds

if(!$wait_timeout)
{
  --let $repeats = 10
}

if($wait_timeout)
{
  --let $repeats = $wait_timeout * 10
}

--source include/count_sessions.inc
if($count_sessions == 1)
{
  --let $repeats=0
}

while($repeats) {
  --sleep 0.1
  --source include/count_sessions.inc
  --dec $repeats
  if($count_sessions == 1)
  {
     --let $repeats=0
  }
}
