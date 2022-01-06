#############################################################################
# Bug #369913: Feature Request: more verbose output during initial table scan
#############################################################################

start_server

xtrabackup --backup --target-dir=$topdir/backup

grep -q "Generating a list of tablespaces" $OUTFILE \
    || die "Could not find \"Generating a list of tablespaces\" message"
