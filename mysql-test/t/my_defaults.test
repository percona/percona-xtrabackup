--echo #
--echo # Bug#26750972: TRAILING # COMMENT ON !INCLUDEDIR / !INCLUDE DIRECTIVE CAUSES ERROR
--echo #

--echo # Create a cnf file with includedir
let $file = $MYSQLTEST_VARDIR/tmp/b26750972.cnf;
--perl
  my $dir= $ENV{'MYSQLTEST_VARDIR'};
  open(FILE, ">", "$dir/tmp/b26750972.cnf");
  print FILE "!includedir $dir/tmp/dir26750972 # comment\n";
  close(FILE)
EOF
--echo # Create a directory for the include
--mkdir $MYSQLTEST_VARDIR/tmp/dir26750972
--echo # Create a cnf file within the include dir
--write_file $MYSQLTEST_VARDIR/tmp/dir26750972/one.cnf
[foo]
user=included_file_user
EOF

--echo # test: must print included_file_user
--exec $MYSQL_MY_PRINT_DEFAULTS --defaults-file=$file foo 2>&1

# Cleanup
--remove_file $MYSQLTEST_VARDIR/tmp/dir26750972/one.cnf
--rmdir $MYSQLTEST_VARDIR/tmp/dir26750972
--remove_file $file

--echo # End of 9.6 tests
