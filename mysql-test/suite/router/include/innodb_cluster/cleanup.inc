DROP SCHEMA mysql_innodb_cluster_metadata;
DROP USER IF EXISTS 'mysqlrouter'@'%';

if ($keyring_master_file)
{
--remove_file $keyring_master_file
}

if ($router_id)
{
--force-rmdir $MYSQL_TMP_DIR/router.$router_id/data/
}
