--disable_query_log
--disable_result_log
# workaround to force the MRS to do the full schema refresh when the next
# tests starts
INSERT INTO mysql_rest_service_metadata.audit_log(id, table_name,
            changed_by, changed_at) VALUES(1000, '', '', NOW());

if (!$PB2WORKDIR)
{
  # Lets run sanity checks @ local machines.
  --let $is_transaction_active=`SELECT count(*) FROM INFORMATION_SCHEMA.INNODB_TRX WHERE trx_mysql_thread_id = CONNECTION_ID()`

  if ($is_transaction_active == 0)
  {
    --die Transaction was closed in middle of object definition, please review scripts/sql used between start/end object definition.
  }
}

COMMIT;
--enable_query_log
--enable_result_log


if ($extra_mrs_router_id)
{
   if ($_mrs_object_definitions > 0)
   {
     --let $mrs_changes_inc=$_mrs_object_definitions
     --source changes/wait_object.inc
   }

   # Default schema has some "options files", add separate
   # counter for it ?
   #--let $mrs_changes_inc=$_mrs_files_definitions
   #--source changes/wait_files.inc
}
