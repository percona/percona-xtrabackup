#
# Bug 1520569: 2.3 creates empty dir after upgrading
#

start_server

# check that directory with timestamp as a name is not created
# (--stream should imply --no-timestamp)
innobackupex --stream=tar $topdir/backup > $topdir/xbs

if [ "$(ls -A $topdir/backup)" ] ; then
	die "Directory is created!"
fi
