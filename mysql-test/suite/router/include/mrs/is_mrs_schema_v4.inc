--source ./check_mrs_schema_version.inc

if ($mrs_schema_version!=4)
{
  --skip Test specific for mrs-metadata-schema version '4', but found '$mrs_schema_version'
}
