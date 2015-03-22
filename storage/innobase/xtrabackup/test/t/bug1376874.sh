########################################################################
# Bug 1376874: --apply-log and --decompress aren't mutually exclusive
#              Test checks different combinatinos of options
########################################################################

run_cmd_expect_failure ${IB_BIN} ${IB_ARGS} --decompress --apply-log backup
run_cmd_expect_failure ${IB_BIN} ${IB_ARGS} --decompress --copy-back backup
run_cmd_expect_failure ${IB_BIN} ${IB_ARGS} --decompress --move-back backup

run_cmd_expect_failure ${IB_BIN} ${IB_ARGS} --decrypt=AES256 --apply-log backup
run_cmd_expect_failure ${IB_BIN} ${IB_ARGS} --decrypt=AES256 --copy-back backup
run_cmd_expect_failure ${IB_BIN} ${IB_ARGS} --decrypt=AES256 --move-back backup

run_cmd_expect_failure ${IB_BIN} ${IB_ARGS} --apply-log --copy-back backup
run_cmd_expect_failure ${IB_BIN} ${IB_ARGS} --apply-log --move-back backup

run_cmd_expect_failure ${IB_BIN} ${IB_ARGS} --copy-back --move-back backup

COUNT=$(grep -E -c -- "--[^ ]+ and --[^ ]+ are mutually exclusive" $OUTFILE)
echo $COUNT
[ "$COUNT" == "9" ]
