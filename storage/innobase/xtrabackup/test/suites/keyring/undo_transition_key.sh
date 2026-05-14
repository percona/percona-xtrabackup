#
# PXB-3762: xtrabackup --backup --transition-key crashes when an undo
# tablespace encryption key exists in redo only (newer than the on-disk
# undo page).
#
# JIRA URL: https://perconadev.atlassian.net/browse/PXB-3762
#
# Strategy:
#   1. Use release-safe server knobs that reduce page flushing pressure.
#   2. Build enough undo/redo activity on an encrypted table.
#   3. Rotate master key immediately before backup.
#   4. Start backup with --transition-key right away.
#
# This widens the race window for "redo key exists and is newer than
# undo-page LSN", which is required to enter Condition 1 in
# srv_undo_tablespace_read_encryption().
#
require_server_version_higher_than 8.4.0

KEYRING_TYPE="component"
. inc/keyring_common.sh
. inc/keyring_file.sh

# Keep dirty pages in memory longer so on-disk undo pages are more likely
# to lag behind redo-encryption records right after key rotation.
MYSQLD_EXTRA_MY_CNF_OPTS="${MYSQLD_EXTRA_MY_CNF_OPTS}
innodb_buffer_pool_size=512M
innodb_redo_log_capacity=512M
innodb_io_capacity=100
innodb_io_capacity_max=200
innodb_max_dirty_pages_pct=95
innodb_max_dirty_pages_pct_lwm=0
"

configure_server_with_component

mysql -e "CREATE TABLE t (
  a BIGINT PRIMARY KEY AUTO_INCREMENT,
  b LONGBLOB
) ENGINE=InnoDB ENCRYPTION='y'" test
mysql -e "INSERT INTO t(b) VALUES (REPEAT(UUID(), 256))" test

for attempt in $(seq 1 5); do
    vlog "=== attempt ${attempt}/5: rotate + backup + prepare + restore ==="

    backup_dir=$topdir/backup.$attempt
    mkdir -p "$backup_dir"

    # Generate undo/redo churn right before rotation.
    for i in $(seq 1 200); do
        mysql -e "INSERT INTO t(b) VALUES (REPEAT(UUID(), 256))" test >/dev/null 2>&1
        mysql -e "DELETE FROM t WHERE a % 13 = 0 ORDER BY a LIMIT 2" test >/dev/null 2>&1
    done

    # Rotate right before backup so redo key LSN can be newer than on-disk undo.
    mysql -e "ALTER INSTANCE ROTATE INNODB MASTER KEY" test

    record_db_state test

    xtrabackup --backup --transition-key=123 --target-dir="$backup_dir"
    xtrabackup --prepare --transition-key=123 --target-dir="$backup_dir"

    stop_server
    rm -rf "$mysql_datadir"
    xtrabackup --copy-back --target-dir="$backup_dir"
    cp "${instance_local_manifest}" "$mysql_datadir"
    cp "${keyring_component_cnf}" "$mysql_datadir"
    start_server

    verify_db_state test
    rm -rf "$backup_dir"
done
