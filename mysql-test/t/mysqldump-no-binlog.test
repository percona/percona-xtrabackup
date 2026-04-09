
# When log-bin, skip-log-bin and binlog-format options are specified, mask the warning.
--disable_query_log
call mtr.add_suppression("\\[Warning\\] \\[[^]]*\\] \\[[^]]*\\] You need to use --log-bin to make --binlog-format work.");
call mtr.add_suppression("A partial dump from a server that has GTIDs");
call mtr.add_suppression("A dump from a server that has GTIDs enabled");
--enable_query_log

--replace_regex /SOURCE_LOG_POS=[0-9]+/XX/
--error 2
--exec $MYSQL_DUMP --compact --source-data=2 test 2>&1
