#
# Basic tests InnoDB page compression
#

. inc/common.sh
. inc/keyring_file.sh

require_server_version_higher_than 5.7.10

function is_sparse_file() {
    let n=$(stat --printf "%b*%B/%s" $1)
    [ "$n" = "0" ]
}

function is_multiple_of_page_size() {
    let n=$(stat --printf "%s" $1)
    remainder=$((n % 16384 ))
    [ "$remainder" -eq 0 ]
}

start_server

cat $MYSQLD_ERRFILE

if grep -q 'PUNCH HOLE support not available' $MYSQLD_ERRFILE ; then
    skip 'punch hole support is not available'
fi

run_cmd $MYSQL $MYSQL_ARGS test <<EOF

CREATE TABLE t1 (c1 BLOB) COMPRESSION='zlib';
CREATE TABLE t2 (c1 BLOB) COMPRESSION='lz4';
CREATE TABLE t3 (c1 BLOB) COMPRESSION='zlib' ENCRYPTION='y';
CREATE TABLE t4 (c1 BLOB) COMPRESSION='lz4' ENCRYPTION='y';
CREATE TABLE t5 (c1 BLOB);
EOF

for i in {1..5} ; do
    run_cmd $MYSQL $MYSQL_ARGS test <<EOF
INSERT INTO t$i VALUES (REPEAT('x', 5000));

INSERT INTO t$i SELECT * FROM t$i;
INSERT INTO t$i SELECT * FROM t$i;
INSERT INTO t$i SELECT * FROM t$i;
INSERT INTO t$i SELECT * FROM t$i;

EOF
done

# wait for InnoDB to flush all dirty pages
innodb_wait_for_flush_all

if is_sparse_file $mysql_datadir/test/t1.ibd ; then
    working_compression="yes"
else
    working_compression="no"
fi

xtrabackup --backup --target-dir=$topdir/backup --transition-key=123

mkdir $topdir/backuplsn
xtrabackup --backup --target-dir=$topdir/tmp --transition-key=123 \
           --extra-lsndir=$topdir/backuplsn \
           --stream=xbstream --compress=lz4 --compress-threads=2 > $topdir/backup.xbs

if [ $working_compression = "yes" ] ; then
    for i in {1..4} ; do
        is_sparse_file $topdir/backup/test/t$i.ibd || die "Backed up t$i.ibd is not sparse"
        is_multiple_of_page_size $topdir/backup/test/t$i.ibd || die "Backed up t$i.ibd is not mulitple of page_size"
    done
fi

for i in {1..5} ; do
    run_cmd $MYSQL $MYSQL_ARGS test <<EOF
INSERT INTO t$i SELECT * FROM t$i;
INSERT INTO t$i SELECT * FROM t$i;
INSERT INTO t$i SELECT * FROM t$i;
INSERT INTO t$i SELECT * FROM t$i;
EOF
done

# wait for InnoDB to flush all dirty pages
innodb_wait_for_flush_all

xtrabackup --backup --target-dir=$topdir/backup1 \
           --incremental-basedir=$topdir/backup --transition-key=123

xtrabackup --backup --target-dir=$topdir/tmp \
           --incremental-basedir=$topdir/backuplsn --transition-key=123 \
           --stream=xbstream --compress=lz4 --compress-threads=2 > $topdir/backup1.xbs

record_db_state test

function restore_and_verify() {
    xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup \
               --transition-key=123

    if [ $working_compression = "yes" ] ; then
        for i in {1..4} ; do
            is_sparse_file $topdir/backup/test/t$i.ibd || die "Prepared t$i.ibd is not sparse"
            is_multiple_of_page_size $topdir/backup/test/t$i.ibd || die "Prepared t$i.ibd is not mulitple of page_size"
        done
    fi

    xtrabackup --prepare --target-dir=$topdir/backup --incremental-dir=$topdir/backup1 \
               --transition-key=123

    if [ $working_compression = "yes" ] ; then
        for i in {1..4} ; do
            is_sparse_file $topdir/backup/test/t$i.ibd || die "Prepared (incremental) t$i.ibd is not sparse"
	    is_multiple_of_page_size $topdir/backup/test/t$i.ibd || die "Prepared (incremental) t$i.ibd is not mulitple of page_size"
        done
    fi

    stop_server

    rm -rf $mysql_datadir

    xtrabackup --copy-back --target-dir=$topdir/backup --transition-key=123 \
               --generate-new-master-key \
               --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args}

    if [ $working_compression = "yes" ] ; then
        for i in {1..4} ; do
            is_sparse_file $topdir/backup/test/t$i.ibd || die "Restored t$i.ibd is not sparse"
            is_multiple_of_page_size $topdir/backup/test/t$i.ibd || die "Restored t$i.ibd is not mulitple of page_size"
        done
    fi

    start_server

    verify_db_state test
}

restore_and_verify

rm -rf $topdir/backup && mkdir $topdir/backup
xbstream -x -v -C $topdir/backup --decompress --decompress-threads=2 < $topdir/backup.xbs

rm -rf $topdir/backup1 && mkdir $topdir/backup1
xbstream -x -v -C $topdir/backup1 --decompress --decompress-threads=2 < $topdir/backup1.xbs

restore_and_verify
