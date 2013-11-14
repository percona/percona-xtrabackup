##########################################################################
# Bug #1247586: xtrabackup_56 defaults to innodb_checksum_algorithm=crc32
##########################################################################

if [ ${MYSQL_VERSION:0:3} != "5.6" ]
then
    echo "Requires a 5.6 server" > $SKIPPED_REASON
    exit $SKIPPED_EXIT_CODE
fi

if ! xtrabackup --help 2>&1 |
       egrep '^innodb-checksum-algorithm[[:space:]]+innodb$'
then
    die "XtraBackup is using an incorrect default value for \
--innodb-checksum-algorithm"
fi
