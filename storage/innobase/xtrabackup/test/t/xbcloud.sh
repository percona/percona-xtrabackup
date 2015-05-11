################################################################################
# Test xbcloud
#
# Set following environment variables to enable this test:
#     XBCLOUD_CREDENTIALS
#
# Example:
#     export XBCLOUD_CREDENTIALS="--storage=swift \
#         --swift-url=http://192.168.8.80:8080/ \
#         --swift-user=test:tester 
#         --swift-key=testing"
#
################################################################################

. inc/common.sh

[ "${XBCLOUD_CREDENTIALS:-unset}" == "unset" ] && \
	skip_test "Requires XBCLOUD_CREDENTIALS"

start_server --innodb_file_per_table

load_dbase_schema sakila
load_dbase_data sakila

now=$(date +%s)

full_backup_name=${now}-full_backup
inc_backup_name=${now}-inc_backup

full_backup_dir=$topdir/${full_backup_name}
inc_backup_dir=$topdir/${inc_backup_name}

vlog "take full backup"

xtrabackup --backup --stream=xbstream --extra-lsndir=$full_backup_dir \
	--target-dir=$full_backup_dir | xbcloud put \
	--swift-container=test_backup \
	${XBCLOUD_CREDENTIALS} \
	--parallel=10 \
	${full_backup_name}

vlog "take incremental backup"

xtrabackup --backup --incremental-basedir=$full_backup_dir \
	--stream=xbstream --target-dir=inc_backup_dir | xbcloud put \
	--swift-container=test_backup \
	${XBCLOUD_CREDENTIALS} \
	${inc_backup_name}

vlog "download and prepare"

mkdir $topdir/downloaded_full
mkdir $topdir/downloaded_inc

run_cmd xbcloud get --swift-container=test_backup \
	${XBCLOUD_CREDENTIALS} \
	${full_backup_name} | xbstream -xv -C $topdir/downloaded_full

xtrabackup --prepare --apply-log-only --target-dir=$topdir/downloaded_full

run_cmd xbcloud get --swift-container=test_backup \
	${XBCLOUD_CREDENTIALS} \
	${inc_backup_name} | xbstream -xv -C $topdir/downloaded_inc

xtrabackup --prepare --apply-log-only \
	--target-dir=$topdir/downloaded_full \
	--incremental-dir=$topdir/downloaded_inc

xtrabackup --prepare --target-dir=$topdir/downloaded_full

# test partial download

mkdir $topdir/partial

xbcloud get --swift-container=test_backup ${XBCLOUD_CREDENTIALS} \
	${full_backup_name} ibdata1 sakila/payment.ibd \
	> $topdir/partial/partial.xbs

xbstream -xv -C $topdir/partial < $topdir/partial/partial.xbs \
				2>$topdir/partial/partial.list

diff -u $topdir/partial/partial.list - <<EOF
ibdata1
sakila/payment.ibd
EOF
