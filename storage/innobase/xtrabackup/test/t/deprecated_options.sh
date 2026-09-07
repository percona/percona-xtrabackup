#
# PXB-3788: options the runtime stopped acting on must say so at startup.
#
# Every option below is still accepted (no script breaks, no exit-code
# change), but passing one now prints a warning naming it. --rsync is the
# odd one out: it is deprecated but still functional, so it must be
# announced as going away rather than as having no effect.
#

start_server

mysql -e "CREATE TABLE t1 (a INT) ENGINE=InnoDB" test
mysql -e "INSERT INTO t1 (a) VALUES (1), (2), (3)" test

dead_options="log innodb create-ib-logfile rebuild_threads"

# --no-version-check only exists in a -DWITH_VERSION_CHECK=ON build.
if $XB_BIN --help 2>&1 | grep -q -- "--no-version-check" ; then
	dead_options="$dead_options no-version-check"
fi

# Control: a backup passing none of them must stay quiet, so any warning
# later in this test can only come from the option actually being set.
xtrabackup --backup --target-dir=$topdir/backup

if grep -q "is deprecated and has no effect" $OUTFILE ; then
	die "deprecation warning emitted for an option that was not passed"
fi

if grep -q -- "--rsync is deprecated" $OUTFILE ; then
	die "--rsync warning emitted when --rsync was not passed"
fi

# --rsync is a --backup option, so exercise it there rather than under
# --prepare. It shells out to the rsync binary, so skip this part when that
# is not installed - same convention as t/ib_rsync.sh and friends.
if which rsync > /dev/null 2>&1 ; then
	xtrabackup --backup --rsync --target-dir=$topdir/backup_rsync

	grep -q -- "--rsync is deprecated and unsupported" $OUTFILE \
		|| die "no deprecation warning for --rsync"

	# The warning has to name the lock whose window rsync shortened.
	grep -q "shorten the FLUSH TABLES WITH READ LOCK window" $OUTFILE \
		|| die "--rsync warning does not name FLUSH TABLES WITH READ LOCK"

	# --rsync really does route the non-InnoDB copy through rsync, so it
	# must never be described as having no effect.
	if grep -q -- "--rsync is deprecated and has no effect" $OUTFILE ; then
		die "--rsync must not be described as having no effect - it still works"
	fi
else
	vlog "rsync is not installed - skipping the --rsync part of this test"
fi

stop_server

# The dead options are never read, so the mode does not matter. Use
# --prepare, which additionally proves --no-version-check now warns outside
# --backup (it used to warn only there).
dead_args=""
for opt in $dead_options ; do
	case $opt in
		rebuild_threads) dead_args="$dead_args --rebuild_threads=4" ;;
		*)               dead_args="$dead_args --$opt" ;;
	esac
done

xtrabackup --prepare --target-dir=$topdir/backup $dead_args

for opt in $dead_options ; do
	grep -q -- "--$opt is deprecated and has no effect" $OUTFILE \
		|| die "no deprecation warning for --$opt"
done

rm -rf $topdir/backup $topdir/backup_rsync
