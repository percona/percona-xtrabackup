start_server

function run_create() {
local name=$1
mysql <<EOF
CREATE TEMPORARY TABLE $name (a INT) ENGINE=InnoDB;
EOF
}

function run_insert() {
local name=$1
local val=$2
mysql <<EOF
INSERT INTO $name VALUES($val);
EOF
}

vlog "Full backup"

run_create test.tmp
run_insert test.tmp 1

xtrabackup --backup \
    --target-dir=$topdir/data/full

vlog "Incremental backup"

run_insert test.tmp 2
run_insert test.tmp 3
run_insert test.tmp 4
run_insert test.tmp 5

xtrabackup --backup \
    --target-dir=$topdir/data/delta \
    --incremental-basedir=$topdir/data/full

vlog "Prepare full"

xtrabackup --prepare --apply-log-only \
    --throttle=40 \
    --target-dir=$topdir/data/full > $topdir/pxb.log 2>&1

vlog "Prepare Increment"
xtrabackup --prepare  \
    --throttle=40 \
    --target-dir=$topdir/data/full --incremental-dir=$topdir/data/delta >> $topdir/pxb.log 2>&1

run_cmd grep 'InnoDB: xtrabackup: Last MySQL binlog file position' $topdir/pxb.log