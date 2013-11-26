################################################################################
# Test innobackupex --version-check

# There's not much we can do to test full functionality in the test suite. Let's
# just test that the percona-version-check file is created
################################################################################

start_server

# VersionCheck looks for a directory where percona-version-check file is created
# in the following order:
#
#    my @vc_dirs = (
#    '/etc/percona',
#    '/etc/percona-toolkit',
#    '/tmp',
#    "$home",
# );
#
# Assume that both /etc/percona and /etc/percona-toolkit either do not exist or
# are not writable. So we'll be testing /tmp/percona-version-check. There's a
# potential race if there are concurrent tests or other tools using VC, but
# there's currently no way to avoid that.

rm -f /tmp/percona-version-check

# VersionCheck is enabled by default, but the test suite adds --no-version-check
# to IB_ARGS so we don't execute it for all tests.
# First test that --no-version-check (with default IB_ARGS) disables the feature

innobackupex --no-timestamp $topdir/backup1

if [ -f /tmp/percona-version-check ]
then
    die "/tmp/percona-version-check has been created with --no-version-check!"
fi

IB_ARGS=`echo $IB_ARGS | sed -e 's/--no-version-check//g'`

innobackupex --no-timestamp $topdir/backup2

if [ ! -f /tmp/percona-version-check ]
then
    die "/tmp/percona-version-check has not been created!"
fi

rm -f /tmp/percona-version-check

if ! grep -q "Executing a version check" $OUTFILE
then
    die "Version check has not been performed."
fi
