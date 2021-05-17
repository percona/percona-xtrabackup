#
# PXB-2496 - backup prepare always uses the daemon_keyring_proxy_plugin without encryption
#

. inc/common.sh
require_server_version_higher_than 8.0.23

start_server

xtrabackup --backup --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup

if grep -q "Shutting down plugin 'daemon_keyring_proxy_plugin'" $OUTFILE
then
    die "daemon_keyring_proxy_plugin was started."
fi

stop_server
rm -rf $topdir/