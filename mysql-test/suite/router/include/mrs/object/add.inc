if(!$mrs_add_object_name)
{
  --die 'mrs_add_object_name' variable is required.
}

if ($mrs_add_object_fields) {
  if(!$mrs_add_object_field_types)
  {
    --die 'mrs_add_object_field_types' variable is required.
  }
}

if(!$mrs_add_object_type)
{
  --die 'mrs_add_object_type' variable is required (one of "RESULT", "PARAMETERS").
}

if (!$mrs_sql_db_object_id_variable)
{
  --let $mrs_sql_db_object_id_variable=@db_object_id
}


if (!$mrs_sql_object_id_variable)
{
  --let $mrs_sql_object_id_variable=@object_id
}

--disable_query_log
--disable_result_log

eval SET $mrs_sql_object_id_variable=(SELECT mysql_rest_service_metadata.get_sequence_id());
eval INSERT INTO `mysql_rest_service_metadata`.`object`(`id`, `db_object_id`, `kind`, `name`)
       VALUES(
          $mrs_sql_object_id_variable,
          $mrs_sql_db_object_id_variable,
          '$mrs_add_object_type',
          '$mrs_add_object_name');

let $name=$mrs_add_object_fields;
let $type=$mrs_add_object_field_types;
let $iteration=1;

while($name)
{
  #
  # split status variable names using "," as delimiter
  #
  let $name_element=`SELECT TRIM(REPLACE(SUBSTRING_INDEX("$name", ",", 1),"\n",""))`;
  let $name=`SELECT TRIM(SUBSTRING("$name", LENGTH("$name_element") + 2))`;

  #
  # split status variable values using "," as delimiter
  #
  let $type_element=`SELECT TRIM(REPLACE(SUBSTRING_INDEX("$type", ",", 1),"\n",""))`;
  let $type=`SELECT TRIM(SUBSTRING("$type", LENGTH("$type_element") + 2))`;

   eval INSERT INTO `mysql_rest_service_metadata`.`object_field`(`id`, `object_id`, `name`, `db_column`, `position`)
          VALUES(
             mysql_rest_service_metadata.get_sequence_id(),
              $mrs_sql_object_id_variable,
              '$name_element',
              JSON_SET("{}",
                  "$.name",'$name_element',
                  "$.in", true,
                  "$.datatype",'$type_element'),
              $iteration
          );
   inc $iteration;
}

--enable_query_log
--enable_result_log


--let $mrs_sql_db_object_id_variable=
--let $mrs_sql_object_id_variable=
--let $mrs_add_object_name=
--let $mrs_add_object_fields=
--let $mrs_add_object_field_types=
--let $mrs_add_object_type=
