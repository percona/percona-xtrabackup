--source include/have_router_bootstrap.inc

--source ../include/mrs/initialize_schema.inc

#create a b/s user with minimal required set of priviledges

CREATE USER mrs_bootstrap_user IDENTIFIED BY 'pass';
GRANT SELECT, INSERT, UPDATE on mysql_rest_service_metadata.* to mrs_bootstrap_user;
GRANT mysql_rest_service_data_provider TO mrs_bootstrap_user WITH ADMIN OPTION;
GRANT CREATE USER ON *.* TO mrs_bootstrap_user WITH GRANT OPTION;
GRANT mysql_rest_service_meta_provider TO mrs_bootstrap_user WITH ADMIN OPTION;
FLUSH PRIVILEGES;


replace_regex
   / Bootstrapping MySQL Router.* instance at '.*'.*/ Bootstrapping MySQL Router instance at 'DIRECTORY'/
   /Creating configuration .*mysqlrouter.conf/Creating configuration ...\/mysqlrouter.conf/
   /.*mysqlrouter.* -c .*mysqlrouter.conf/$ mysqlrouter -c ...\/mysqlrouter.conf/
   /Adjusting configuration file .*mysqlrouter.conf/Adjusting configuration file ...\/mysqlrouter.conf/
   /- Adjusting permissions of generated files\n//
   ;

exec $MYSQLROUTER_BOOTSTRAP
   $bootstrap_name_cmd
   --conf-set-option=DEFAULT.plugin_folder=$ROUTER_PLUGIN_DIRECTORY
   -B mrs_bootstrap_user:pass@127.0.0.1:$MASTER_MYPORT
   --directory $MYSQL_TMP_DIR/bootstrap_folder/
   --mrs --mrs-global-secret something_important ;

replace_regex /=.*\/bootstrap_folder\/.*/=TEMP_DIR_FILE_OR_FOLDER/
                /mysql_user=mysql_router_mrs1_.*/mysql_user=mysql_router_mrs1_RANDOM/
                /user=mysql_router1_.*/user=mysql_router1_RANDOM/
                /plugin_folder=.*/plugin_folder=ROUTER_PLUGIN_DIRECTORY/
                /destinations=.*/destinations=ADDR:PORT/ ;


# Check the contents of the configuration file created
--cat_file $MYSQL_TMP_DIR/bootstrap_folder/mysqlrouter.conf

# Check the metadata registered during the bootstrap
if ($bootstrap_name_cmd) {
  select id, router_name, product_name, options from mysql_rest_service_metadata.router;
}
if (!$bootstrap_name_cmd) {
  select id, product_name, options from mysql_rest_service_metadata.router;

  --replace_regex /^.+:\d+$/"Name found"/
  select router_name  from mysql_rest_service_metadata.router;
}

--replace_regex /"mysql_router_mrs1_.*"/"mysql_router_mrs1_RANDOM"/
select attributes from mysql_rest_service_metadata.router;

# Cleanup
if (!$KEEP_ROUTER_FILES)
{
  --disable_query_log
  --disable_result_log
  --let $u1=`SELECT user from mysql.user WHERE user like "mysql_router_mrs1_%";`
  eval DROP USER $u1@'%';
  --enable_query_log
  --enable_result_log

  DROP USER 'mrs_bootstrap_user'@'%';
  --force-rmdir $MYSQL_TMP_DIR/bootstrap_folder
}

if ($KEEP_ROUTER_FILES == 1)
{
  --echo DROP USER 'mrs_bootstrap_user'@'%';
}

--let $cleanup_arg_mrs_skip_service_disable=1
--source ../include/mrs/cleanup.inc

