--source ./check_mrs_schema_version.inc

if ($mrs_schema_version!=2)
{
  --skip Test specific for mrs-metadata-schema version '2' found '$mrs_schema_version'
}
