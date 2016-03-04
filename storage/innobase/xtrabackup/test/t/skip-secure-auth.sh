#
# Option --[skip]-secure-auth
#

require_server_version_lower_than 5.6.0

start_server

run_cmd $MYSQL $MYSQL_ARGS <<EOF

SET SESSION old_passwords= 1;

CREATE USER 'pxb'@'localhost';
SET PASSWORD FOR 'pxb'@'localhost' = PASSWORD('123456');
GRANT ALL PRIVILEGES ON *.* TO 'pxb'@'localhost' WITH GRANT OPTION;

EOF

run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --target-dir=$topdir/backup \
					--user=pxb --password=123456

xtrabackup --backup --target-dir=$topdir/backup --user=pxb --password=123456 \
	   --skip-secure-auth

run_cmd_expect_failure $IB_BIN $IB_ARGS --user=pxb \
					--password=123456 $topdir/backup

innobackupex --user=pxb --password=123456 --skip-secure-auth $topdir/backup
