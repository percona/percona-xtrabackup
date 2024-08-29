# Percona XtraBackup User Manual

*Percona XtraBackup* is a set of the following tools:

[xtrabackup](https://docs.percona.com/percona-xtrabackup/8.0/xtrabackup_bin/xtrabackup_binary.html)

    a compiled *C* binary that provides functionality to backup a whole *MySQL*
    database instance with *MyISAM*, *InnoDB*, and *XtraDB* tables.

[xbcrypt](https://docs.percona.com/percona-xtrabackup/8.0/xbcrypt/xbcrypt.html)

    utility used for encrypting and decrypting backup files.

[xbstream](https://docs.percona.com/percona-xtrabackup/8.0/xbstream/xbstream.html)

    utility that allows streaming and extracting files to/from the
    xbstream format.

[xbcloud](https://docs.percona.com/percona-xtrabackup/8.0/xbcloud/xbcloud.html)

    utility used for downloading and uploading full or part of *xbstream*
    archive from/to cloud.

The recommended way to take the backup is
by using the *xtrabackup* script. For more information on script 
options, see [xtrabackup](https://docs.percona.com/percona-xtrabackup/8.0/xtrabackup_bin/xtrabackup_binary.html).
