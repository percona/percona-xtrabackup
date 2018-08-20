########################################################################
# Bug 1376874: --apply-log and --decompress aren't mutually exclusive
#              Test checks different combinatinos of options
########################################################################

run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --decompress --prepare --target-dir=backup
run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --decompress --copy-back --target-dir=backup
run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --decompress --move-back --target-dir=backup

run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --decrypt=AES256 --prepare --target-dir=backup
run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --decrypt=AES256 --copy-back --target-dir=backup
run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --decrypt=AES256 --move-back --target-dir=backup

run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --prepare --copy-back --target-dir=backup
run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --prepare --move-back --target-dir=backup

run_cmd_expect_failure ${XB_BIN} ${XB_ARGS} --copy-back --move-back --target-dir=backup

COUNT=$(grep -E -c -- "--[^ ]+ and --[^ ]+ are mutually exclusive" $OUTFILE)
echo $COUNT
[ "$COUNT" == "9" ]
