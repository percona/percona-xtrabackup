. inc/common.sh

start_server --innodb_file_per_table

load_dbase_schema sakila
load_dbase_data sakila

full_backup_dir=$topdir/full_backup
part_backup_dir=$topdir/part_backup

vlog "take full backup"

innobackupex --stream=xbstream $full_backup_dir --extra-lsndir=$full_backup_dir | xbcloud put --storage=Swift \
	--swift-container=test \
	--swift-user=test:tester \
	--swift-url=http://192.168.8.80:8080/ \
	--swift-key=testing \
	--parallel=10 \
	--verbose \
	full_backup

vlog "take incremental backup"

inc_lsn=`grep to_lsn $full_backup_dir/xtrabackup_checkpoints | \
             sed 's/to_lsn = //'`

[ -z "$inc_lsn" ] && die "Couldn't read to_lsn from xtrabackup_checkpoints"

innobackupex --incremental --incremental-lsn=$inc_lsn --stream=xbstream part_backup_dir | xbcloud put --storage=Swift \
	--swift-container=test \
	--swift-user=test:tester \
	--swift-url=http://192.168.8.80:8080/ \
	--swift-key=testing \
	incremental

vlog "download and prepare"

mkdir $topdir/downloaded_full
mkdir $topdir/downloaded_inc

xbcloud get --storage=Swift \
	--swift-container=test \
	--swift-user=test:tester \
	--swift-url=http://192.168.8.80:8080/ \
	--swift-key=testing \
	full_backup | xbstream -xv -C $topdir/downloaded_full

innobackupex --apply-log --redo-only $topdir/downloaded_full

xbcloud get --storage=Swift \
	--swift-container=test \
	--swift-user=test:tester \
	--swift-url=http://192.168.8.80:8080/ \
	--swift-key=testing \
	incremental | xbstream -xv -C $topdir/downloaded_inc

innobackupex --apply-log --redo-only $topdir/downloaded_full --incremental-dir=$topdir/downloaded_inc

innobackupex --apply-log $topdir/downloaded_full
