########################################################################
# Tests for distribution specific messages in innobackupex
########################################################################

[ -z ${XB_DISTRIBUTION:-} ] && skip_test "Requires XB_DISTRIBUTION to be set"

innobackupex --version

link="http://www.percona.com/xb/$XB_DISTRIBUTION"

if ! grep -q $link $OUTFILE
then
    die "Could not find '$link' in the innobackupex version message!"
fi
