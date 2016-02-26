########################################################################
# Bug 1388122: Innobackupex does not work with .mylogin.cnf
########################################################################

require_server_version_higher_than 5.6.0

start_server

export HOME=$topdir/home

rm -rf $HOME/.mylogin.cnf
mkdir -p $HOME

mysql -e "CREATE USER 'backup'@'localhost' IDENTIFIED BY 'secret'"
mysql -e "GRANT ALL PRIVILEGES ON * . * TO 'backup'@'localhost'"

run_cmd mysql_config_editor \
	set --login-path=backup --user=backup --host=localhost

innobackupex --no-timestamp --login-path=backup --password=secret $topdir/backup

rm -rf $HOME/.mylogin.cnf
