--source ./check_mrs_schema_version.inc

if ($mrs_schema_version!=2)
{
  if ($mrs_schema_version!=3)
  {
    --skip Test specific for mrs-metadata-schema version '2' and '3', but found '$mrs_schema_version'
  }
}
