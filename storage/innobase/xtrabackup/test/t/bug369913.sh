#############################################################################
# Bug #369913: Feature Request: more verbose output during initial table scan
#############################################################################

start_server

innobackupex --no-timestamp $topdir/backup

egrep -q \
      '[0-9]{6} [0-9]{2}:[0-9]{2}:[0-9]{2} .* Starting the backup operation' \
      $OUTFILE \
    || die "Could not find the operation start message"

grep -q "xtrabackup: Generating a list of tablespaces" $OUTFILE \
    || die "Could not find \"Generating a list of tablespaces\" message"
