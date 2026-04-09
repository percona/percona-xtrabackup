# ==== Purpose ====

# get details from mysql_rest_service_metadata.router_status table

# Wait for the latest counter values.
--let $_mrs_definition_start=`SELECT NOW()`
--let $wait_condition=SELECT status_time>"$_mrs_definition_start" FROM mysql_rest_service_metadata.router_status WHERE router_id=$extra_mrs_router_id ORDER BY id DESC LIMIT 1;
--source include/wait_condition_or_abort.inc

if (!$_status_wait_last_id)
{
  --die Please first include mysqlrouter_reset_status.inc
}

--disable_query_log

# MAX is need for restCachedEndpoints, restCachedItems, restCachedFiles, those are not resetable counters.
# for others SUM.
eval select json_object(
        'endpoints', MAX(details->'$.restCachedEndpoints'),
        'items', MAX(details->'$.restCachedItems'),
        'files', MAX(details->'$.restCachedFiles'),
        'itemLoads', SUM(details->'$.restCacheItemLoads'),
        'itemEjects', SUM(details->'$.restCacheItemEjects'),
        'itemHits', SUM(details->'$.restCacheItemHits'),
        'itemMisses', SUM(details->'$.restCacheItemMisses'),
        'fileLoads', SUM(details->'$.restCacheFileLoads'),
        'fileEjects', SUM(details->'$.restCacheFileEjects'),
        'fileHits', SUM(details->'$.restCacheFileHits'),
        'fileMisses', SUM(details->'$.restCacheFileMisses')
        ) as stats from mysql_rest_service_metadata.router_status WHERE  id > $_status_wait_last_id;
--enable_query_log

--let $_mrs_definition_start=
--let $_status_wait_last_id=
