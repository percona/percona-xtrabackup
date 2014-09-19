##########################################################################
# Bug #1213778: --incremental-lsn without --incremental does not return an
#               error.
##########################################################################

innobackupex --incremental-lsn=0 $topdir 2>&1 |
    grep 'require the --incremental option'

innobackupex --incremental-basedir=$topdir $topdir 2>&1 |
    grep 'require the --incremental option'

innobackupex --incremental-history-name=foo $topdir 2>&1 |
    grep 'require the --incremental option'

innobackupex --incremental-history-uuid=f81d4fae-7dec-11d0-a765-00a0c91e6bf6 \
             $topdir 2>&1 |
    grep 'require the --incremental option'
