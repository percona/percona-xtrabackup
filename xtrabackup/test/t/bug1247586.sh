##########################################################################
# Bug #1247586: xtrabackup_56 defaults to innodb_checksum_algorithm=crc32
##########################################################################

require_server_version_higher_than 5.6.0

if ! xtrabackup --help 2>&1 |
       egrep '^innodb-checksum-algorithm[[:space:]]+innodb$'
then
    die "XtraBackup is using an incorrect default value for \
--innodb-checksum-algorithm"
fi
