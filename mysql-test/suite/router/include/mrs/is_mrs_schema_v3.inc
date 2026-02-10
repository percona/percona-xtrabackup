--source ./check_mrs_schema_version.inc

if ($mrs_schema_version!=3)
{
    --skip Test specific for mrs-metadata-schema version '3' found '$mrs_schema_version'
}
