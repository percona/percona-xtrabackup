#
# PXB-1493: XtraBackup does not check validity on arguments used.
#

start_server

err=$(${XB_BIN} ${XB_ARGS} --strict --backup --skip-myoption --target-dir=$topdir/backup 2>&1 | grep "unknown option")
if [ -z "$err" ] ; then
    die "xtrabackup did not report error (--myoption)"
fi

err=$(${XB_BIN} ${XB_ARGS} --strict --backup --compress --copmress-threads=4 --target-dir=$topdir/backup 2>&1 | grep "unknown variable")
if [ -z "$err" ] ; then
    die "xtrabackup did not report error (--copmress-threads)"
fi

stop_server

XB_EXTRA_MY_CNF_OPTS="
myvariable=123
"
start_server

err=$(${XB_BIN} ${XB_ARGS} --strict --backup --target-dir=$topdir/backup 2>&1 | grep "unknown variable")
if [ -z "$err" ] ; then
    die "xtrabackup did not report error (--myvariable)"
fi

err=$(${XB_BIN} ${XB_ARGS} --backup --skip-myoption --target-dir=$topdir/backup 2>&1 | grep "WARNING: unknown option")
if [ -z "$err" ] ; then
    die "xtrabackup did not report warning (--myoption)"
fi

echo all ok
