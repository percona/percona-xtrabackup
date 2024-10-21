#
# PXB-1954: PXB gets stuck in prepare of full backup for encrypted tablespace
#

KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component

mysql test <<EOF
CREATE TABLE t (a INT) ENCRYPTION='y';
EOF

mysql -e "INSERT INTO t (a) VALUES (1), (2), (3), (4), (5), (6), (7), (8), (9)" test

mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test
mysql -e "INSERT INTO t SELECT * FROM t" test

( while true ; do
      mysql -e "ALTER TABLE t ROW_FORMAT=COMPRESSED;" test
      mysql -e "ALTER TABLE t ROW_FORMAT=DYNAMIC;" test
      mysql -e "ALTER TABLE t ROW_FORMAT=COMPACT;" test
      mysql -e "ALTER TABLE t ROW_FORMAT=REDUNDANT;" test
  done ) >/dev/null 2>/dev/null &

xtrabackup --backup --target-dir=$topdir/backup \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

xtrabackup --prepare --target-dir=$topdir/backup \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}
