

if (!$mrs_object_def_content_set_id)
{
  --let $mrs_object_def_content_set_id=@content_set_id
}

if (!$mrs_object_def_db_object_id)
{
  --let $mrs_object_def_db_object_id=@db_object_id
}

if (!$mrs_object_def_kind)
{
  --die 'mrs_object_def_kind' variable is required.
}

if (!$mrs_object_def_language)
{
  --die 'mrs_object_def_language' variable is required.
}

if (!$mrs_object_def_name)
{
  --die 'mrs_object_def_name' variable is required.
}

if (!$mrs_object_def_class_name)
{
  --die 'mrs_object_def_class_name' variable is required.
}

if (!$mrs_object_def_options)
{
  --die 'mrs_object_def_options' variable is required.
}


--disable_query_log
--disable_result_log

eval  INSERT INTO mysql_rest_service_metadata.content_set_has_obj_def(
               content_set_id, db_object_id, kind, language,
               name, class_name, options)
      VALUES($mrs_object_def_content_set_id,
             $mrs_object_def_db_object_id,
             "$mrs_object_def_kind",
             "$mrs_object_def_language",
             "$mrs_object_def_name",
             "$mrs_object_def_class_name",
             $mrs_object_def_options);

--echo # Registered CONTENT_SET Object Definition for: $mrs_object_def_class_name.$mrs_object_def_name

--enable_query_log
--enable_result_log

--let $mrs_object_def_content_set_id=
--let $mrs_object_def_db_object_id=
--let $mrs_service_id_sql_variable=
--let $mrs_object_def_kind=
--let $mrs_object_def_language=
--let $mrs_object_def_name=
--let $mrs_object_def_class_name=
--let $mrs_object_def_options=
