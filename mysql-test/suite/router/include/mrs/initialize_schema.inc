# ==== Purpose ====
#
# Create valid MySQL Rest Service schema.
#
# ==== Usage ====
#
# --source ../include/mrs_initialize_schema.inc
#

--disable_query_log
--disable_result_log

--source ./check_mrs_schema_version.inc

if ($mrs_schema_version == 4)
{
  --source ../schema/mrs_metadata_schema-version4.sql
  --let $_mrs_schema_version_supported=1
}


if ($mrs_schema_version == 3)
{
  --source ../schema/mrs_metadata_schema-version3.sql
  --let $_mrs_schema_version_supported=1
}

if ($mrs_schema_version == 2)
{
  --source ../schema/mrs_metadata_schema-version2.sql
  --let $_mrs_schema_version_supported=1
}

if (!$_mrs_schema_version_supported)
{
  --die MRS test-suite executed with unsupported schema version $mrs_schema_version.
}

UPDATE mysql_rest_service_metadata.config SET data = JSON_MERGE_PATCH(data, '{"sqlQuery":{"timeout":60000}}');

--enable_query_log
--enable_result_log

echo # MySQL Rest Service schema created.;
