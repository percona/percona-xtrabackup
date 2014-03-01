########################################################################
# Bug #1273207: SIGPIPE handling of xbstream
########################################################################

# Create a truncated xbstream file containing only a valid chunk header
echo "something" > $TEST_VAR_ROOT/file
xbstream -c $TEST_VAR_ROOT/file | head -c 14 > $TEST_VAR_ROOT/file.xbstream

# Write chunk header and then close the pipe. xbstream would hang before the fix
cat $TEST_VAR_ROOT/file.xbstream | (xbstream -x || true)

rm -f $TEST_VAR_ROOT/{file,file.xbstream}
