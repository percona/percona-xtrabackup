if (!$mrs_check_statements_history_sql_text)
{
    --die Variable '$mrs_check_statements_history_sql_text' must be set
}
if (!$mrs_check_statements_history_delta)
{
    --let $mrs_check_statements_history_delta=0
}

if ($mrs_check_statements_history_initial)
{
    --eval select max(TIMER_END) into @initial_time from performance_schema.events_statements_history_long
}

if (!$mrs_check_statements_history_initial)
{
    --let $events_since_snapshot=`select count(*) from performance_schema.events_statements_history_long where sql_text like $mrs_check_statements_history_sql_text and thread_id<>ps_current_thread_id() and timer_start > @initial_time`
    --echo $events_since_snapshot==$mrs_check_statements_history_delta
    --assert($events_since_snapshot==$mrs_check_statements_history_delta)
}

--let $mrs_check_statements_history_snapshot=
--let $mrs_check_statements_history_delta=