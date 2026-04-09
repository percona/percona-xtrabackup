if (!$script_file)
{
    if (!$default_script_file) {
        --die either 'script_file' or 'default_script_file' variables are required.
    }

    --let $script_file=$default_script_file
}

if (!$script_class_name)
{
    if (!$default_script_class_name) {
        --die either 'script_class_name' or 'default_script_class_name' variable is required.
    }

    --let $script_class_name=$default_script_class_name
}

if (!$script_function_name)
{
  --die 'script_function_name' variable is required.
}



# DB Object
--let $mrs_add_db_object=$script_function_name
--let $mrs_add_db_object_type=SCRIPT
--source ../include/mrs/db_object/add.inc

# Content Set Object Definition
--let $mrs_object_def_kind=Script
--let $mrs_object_def_language=JavaScript
--let $mrs_object_def_class_name=$script_class_name
--let $mrs_object_def_name=$script_function_name
--let $mrs_object_def_options='{"file_to_load":$script_file}'
--source ../include/mrs/content_set/add_object_def.inc

# Object Parameters and Result
--let $mrs_add_object_name=$script_class_name$script_function_nameParams
--let $mrs_add_object_type=PARAMETERS
--source ../include/mrs/object/add.inc

--let $mrs_add_object_name=$script_class_name$script_function_nameResult
--let $mrs_add_object_type=RESULT
--source ../include/mrs/object/add.inc


# Cleanup
--let $script_class_name=
--let $script_function_name=
--let $script_file=
