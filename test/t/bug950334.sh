########################################################################
# Bug #950334: disk space grows by >500% while preparing a backup
########################################################################

. inc/common.sh

start_server --innodb_file_per_table

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE t (a INT) ENGINE=InnoDB;
INSERT INTO t VALUES (1), (2), (3);
EOF

innobackupex --no-timestamp $topdir/backup

innobackupex --apply-log $topdir/backup

# The size of t.ibd should be 96K, not 1M
size_src=`ls -l $MYSQLD_DATADIR/test/t.ibd | awk '{print $5}'`
size_dst=`ls -l $topdir/backup/test/t.ibd | awk '{print $5}'`

if [ "$size_src" != "$size_dst" ]; then
    vlog "t.ibd has different sizes in source ($size_src bytes) and \
backup ($size_dst) directories"
    exit -1
fi
