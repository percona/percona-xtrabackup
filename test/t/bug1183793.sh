########################################################################
# Bug #1183793: XtraBackup should raise the open files limit if possible
########################################################################

MYSQLD_EXTRA_MY_CNF_OPTS="
open_files_limit=1234
"

start_server

innobackupex --no-timestamp $topdir/backup

grep -q 'open files limit requested 1234' $OUTFILE \
    || die "Could not set the open files limit"
