################################################################################
# Test innobackupex --version-check

# There's not much we can do to test full functionality in the test suite. Let's
# just test that the percona-version-check file is created
################################################################################

start_server

# Override the directory where percona-version-check is created to make the test
# stable

export PTDEBUG_VERSION_CHECK_HOME=$topdir

vc_file=$PTDEBUG_VERSION_CHECK_HOME/percona-version-check

[ ! -f $vc_file ] || die "$vc_file exists!"

# VersionCheck is enabled by default, but the test suite adds --no-version-check
# to IB_ARGS so we don't execute it for all tests.
# First test that --no-version-check (with default IB_ARGS) disables the feature

xtrabackup --backup --target-dir=$topdir/backup1

if [ -f $vc_file ]
then
    die "$vc_file has been created with --no-version-check!"
fi

XB_ARGS=`echo $XB_ARGS | sed -e 's/--no-version-check//g'`

xtrabackup --backup --target-dir=$topdir/backup2

if [ ! -f $vc_file ]
then
    die "$vc_file has not been created!"
fi

if ! grep -q "Executing a version check" $OUTFILE
then
    die "Version check has not been performed."
fi

################################################################################
# Bug #1256942: xbstream stream mode issues a warning on standard output messing
#               up the backup stream
################################################################################

mkdir $topdir/backup3

rm -f $vc_file

xtrabackup --backup --stream=xbstream --target-dir=$topdir/backup3 | \
  xbstream -C $topdir/backup3 -xv
