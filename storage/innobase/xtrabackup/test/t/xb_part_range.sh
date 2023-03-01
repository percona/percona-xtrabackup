. inc/common.sh
. inc/ib_part.sh

start_server

require_partitioning

run_cmd $MYSQL $MYSQL_ARGS test <<EOF
CREATE TABLE test (
  a int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1
PARTITION BY RANGE (a)
(PARTITION p0 VALUES LESS THAN (100) ENGINE = InnoDB,
 PARTITION P1 VALUES LESS THAN (200) ENGINE = InnoDB,
 PARTITION p2 VALUES LESS THAN (300) ENGINE = InnoDB,
 PARTITION p3 VALUES LESS THAN (400) ENGINE = InnoDB,
 PARTITION p4 VALUES LESS THAN MAXVALUE ENGINE = InnoDB);
EOF

# Adding 10k rows

vlog "Adding initial rows to database..."

numrow=500
count=0
while [ "$numrow" -gt "$count" ]
do
	${MYSQL} ${MYSQL_ARGS} -e "insert into test values ($count);" test
	let "count=count+1"
done


vlog "Initial rows added"

# Full backup

# Full backup folder
mkdir -p $topdir/backup/full
# Incremental data
mkdir -p $topdir/backup/delta

vlog "Starting backup"

xtrabackup --no-defaults --datadir=$mysql_datadir --backup \
    --target-dir=$topdir/backup/full

vlog "Full backup done"

# Changing data in sakila

vlog "Making changes to database"

numrow=500
count=0
while [ "$numrow" -gt "$count" ]
do
	${MYSQL} ${MYSQL_ARGS} -e "insert into test values ($count);" test
	let "count=count+1"
done

vlog "Changes done"

# Saving the checksum of original table
checksum_a=`checksum_table test test`

vlog "Table checksum is $checksum_a - before backup"

vlog "Making incremental backup"

# Incremental backup
xtrabackup --no-defaults --datadir=$mysql_datadir --backup \
    --target-dir=$topdir/backup/delta --incremental-basedir=$topdir/backup/full

vlog "Incremental backup done"
vlog "Preparing backup"

# Prepare backup
xtrabackup --no-defaults --datadir=$mysql_datadir --prepare --apply-log-only \
    --target-dir=$topdir/backup/full
vlog "Log applied to backup"
xtrabackup --no-defaults --datadir=$mysql_datadir --prepare \
    --target-dir=$topdir/backup/full --incremental-dir=$topdir/backup/delta

# removing rows
vlog "Table cleared"
${MYSQL} ${MYSQL_ARGS} -e "delete from test" test

# Restore backup

stop_server

vlog "Copying files"

rm -rf $mysql_datadir
xtrabackup --datadir=$mysql_datadir --copy-back \
	--target-dir=$topdir/backup/full

vlog "Data restored"

start_server

vlog "Cheking checksums"
checksum_b=`checksum_table test test`

if [ "$checksum_a" != "$checksum_b"  ]
then 
	vlog "Checksums are not equal"
	exit -1
fi

vlog "Checksums are OK"
