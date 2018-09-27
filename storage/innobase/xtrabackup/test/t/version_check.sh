################################################################################
# Test innobackupex --version-check

# There's not much we can do to test full functionality in the test suite. Let's
# just test that the percona-version-check file is created
################################################################################

( $XB_BIN --help 2>/dev/null | grep -q -- --no-version-check ) || \
    skip_test "Requres version check"

start_server

# Override the directory where percona-version-check is created to make the test
# stable

export PTDEBUG_VERSION_CHECK_HOME=$topdir

vc_file=$PTDEBUG_VERSION_CHECK_HOME/percona-version-check

[ ! -f $vc_file ] || die "$vc_file exists!"

# VersionCheck is enabled by default, but the test suite adds --no-version-check
# to IB_ARGS so we don't execute it for all tests.
# First test that --no-version-check (with default IB_ARGS) disables the feature

innobackupex --no-timestamp $topdir/backup1

if [ -f $vc_file ]
then
    die "$vc_file has been created with --no-version-check!"
fi

IB_ARGS=`echo $IB_ARGS | sed -e 's/--no-version-check//g'`

innobackupex --no-timestamp $topdir/backup2

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

innobackupex --no-timestamp --stream=tar $topdir/backup3 | \
  $TAR -C $topdir/backup3 -xivf -
