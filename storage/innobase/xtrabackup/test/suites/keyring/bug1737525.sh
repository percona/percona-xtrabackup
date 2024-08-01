############################################################################
# Full and Incremental backup of encrypted tablespaces
############################################################################

require_server_version_higher_than 8.0.15

KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh
configure_server_with_component


function insert_char()
{
    table=$1
    ( for i in {1..1000} ; do
        echo "INSERT INTO ${table} VALUES (UUID());"
    done ) | mysql test
    local rc=$?
    if [ $rc -ne 0 ]; then
      die "===> insert_char($table) failed with exit code $rc"
    fi
}

function insert_int()
{
    table=$1
    ( for i in {1..1000} ; do
        echo "INSERT INTO ${table} VALUES ($i);"
    done ) | mysql test
    local rc=$?
    if [ $rc -ne 0 ]; then
      die "===> insert_int($table) failed with exit code $rc"
    fi
}

function insert_int2()
{
    table=$1
    ( for i in {1..1000} ; do
        echo "INSERT INTO ${table} VALUES ($i, $i);"
    done ) | mysql test
    local rc=$?
    if [ $rc -ne 0 ]; then
      die "===> insert_int2($table) failed with exit code $rc"
    fi
}



mysql -e "CREATE TABLESPACE ts_encrypted ADD DATAFILE 'ts_encrypted.ibd' ENCRYPTION='Y' ENGINE='InnoDB'" test
mysql -e "CREATE TABLESPACE ts_unencrypted ADD DATAFILE 'ts_unencrypted.ibd' ENGINE='InnoDB'" test
mysql -e "CREATE TABLESPACE ts_encrypted_new ADD DATAFILE 'ts_encrypted_new.ibd' ENCRYPTION='Y' ENGINE='InnoDB'" test

mysql -e "CREATE TABLE t3 (a TEXT) TABLESPACE ts_encrypted ENGINE='InnoDB' ENCRYPTION='y'" test
insert_char t3

mysql -e "CREATE TABLE pt2 (a INT NOT NULL, PRIMARY KEY(a)) \
    ENGINE=InnoDB ENCRYPTION='y' \
    PARTITION BY RANGE (a) PARTITIONS 3 ( \
        PARTITION p1 VALUES LESS THAN (200), \
        PARTITION p2 VALUES LESS THAN (600), \
        PARTITION p3 VALUES LESS THAN (1800))" test

mysql -e "ALTER TABLE pt2 ADD PARTITION (PARTITION p4 VALUES LESS THAN (80000))" test

insert_int pt2

mysql -e "CREATE TABLE spt2 (a INT NOT NULL, b INT) \
    ENGINE=InnoDB ENCRYPTION='y' \
    PARTITION BY RANGE (a) PARTITIONS 3 SUBPARTITION BY KEY (b) ( \
        PARTITION p1 VALUES LESS THAN (200) ( \
            SUBPARTITION p11, \
            SUBPARTITION p12, \
            SUBPARTITION p13), \
        PARTITION p2 VALUES LESS THAN (600) ( \
            SUBPARTITION p21, \
            SUBPARTITION p22, \
            SUBPARTITION p23), \
        PARTITION p3 VALUES LESS THAN (1800) ( \
            SUBPARTITION p31, \
            SUBPARTITION p32, \
            SUBPARTITION p33))" test


insert_int2 spt2

# wait for InnoDB to flush all dirty pages
innodb_wait_for_flush_all


# Full backup
# backup root directory
vlog "Starting backup"

full_backup_dir=$topdir/full_backup
xtrabackup --datadir=$mysql_datadir --backup \
    --target-dir=$full_backup_dir \
    $keyring_args

record_db_state test

# Destroying mysql data
stop_server
rm -rf $mysql_datadir/*
vlog "Data destroyed"

# Restore backup
vlog "Preparing backup"
xtrabackup --datadir=$mysql_datadir --prepare \
    --target-dir=$full_backup_dir \
    --xtrabackup-plugin-dir=$plugin_dir \
    $keyring_args

vlog "Copying files to their original locations"
xtrabackup --copy-back \
    --target-dir=$full_backup_dir \
    --xtrabackup-plugin-dir=$plugin_dir \
    $keyring_args

vlog "Data restored"

cp ${instance_local_manifest}  $mysql_datadir
cp ${keyring_component_cnf} $mysql_datadir

start_server

verify_db_state test


# Take another full backup (for the incremental test)
vlog "Starting backup"
rm -rf "$full_backup_dir"
xtrabackup --datadir=$mysql_datadir --backup \
    --target-dir=$full_backup_dir \
    $keyring_args

# Changing data
insert_char t3
mysql -e "ALTER TABLE spt2 ADD PARTITION (PARTITION p5 VALUES LESS THAN (2400) ( \
            SUBPARTITION p51, \
            SUBPARTITION p52, \
            SUBPARTITION p53))" test

mysql -e "ALTER TABLE spt2 ADD PARTITION (PARTITION p6 VALUES LESS THAN (15000) ( \
            SUBPARTITION p61, \
            SUBPARTITION p62, \
            SUBPARTITION p63))" test
insert_int2 spt2


# Incremental backup
vlog "Making incremental backup"
inc_backup_dir=$topdir/incremental_backup
xtrabackup --datadir=$mysql_datadir --backup \
    --target-dir=$inc_backup_dir \
    --incremental-basedir=$full_backup_dir \
    $keyring_args

vlog "Incremental backup created in directory $inc_backup_dir"

# More changes
mysql -e "CREATE TABLE t4 (a TEXT) TABLESPACE ts_encrypted ENGINE='InnoDB' ENCRYPTION='y'" test
insert_char t4

mysql -e "DROP TABLE t3" test

# Incremental backup
vlog "Making incremental backup"
inc_backup_dir2=$topdir/incremental_backup2
xtrabackup --datadir=$mysql_datadir --backup \
    --target-dir=$inc_backup_dir2 \
    --incremental-basedir=$inc_backup_dir \
    $keyring_args

vlog "Incremental backup 2 created in directory $inc_backup_dir2"

record_db_state test

# Restoring backup
vlog "Preparing backup"
xtrabackup --datadir=$mysql_datadir --prepare \
    --apply-log-only \
    --target-dir=$full_backup_dir \
    --xtrabackup-plugin-dir=$plugin_dir \
    $keyring_args
vlog "Log applied to full backup"

xtrabackup --datadir=$mysql_datadir --prepare \
    --apply-log-only \
    --target-dir=$full_backup_dir \
    --incremental-dir=$inc_backup_dir \
    --xtrabackup-plugin-dir=$plugin_dir \
    $keyring_args
vlog "Delta applied to full backup"

xtrabackup --datadir=$mysql_datadir --prepare \
    --target-dir=$full_backup_dir \
    --incremental-dir=$inc_backup_dir2 \
    --xtrabackup-plugin-dir=$plugin_dir \
    $keyring_args
vlog "Delta2 applied to full backup"

vlog "Data prepared for restore"


# Destroying mysql data
stop_server
rm -rf $mysql_datadir/*
vlog "Data destroyed"

# Restore backup
vlog "Copying files to their original locations"
xtrabackup --copy-back \
    --target-dir=$full_backup_dir \
    --xtrabackup-plugin-dir=$plugin_dir \
    $keyring_args

vlog "Data restored"

cp ${instance_local_manifest}  $mysql_datadir
cp ${keyring_component_cnf} $mysql_datadir

start_server

verify_db_state test

