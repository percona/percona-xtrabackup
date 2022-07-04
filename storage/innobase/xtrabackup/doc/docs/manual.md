# Percona XtraBackup User Manual

*Percona XtraBackup* is a set of following tools:

[innobackupex](innobackupex/innobackupex_script.md)

    innobackupex is the symlink for *xtrabackup*. innobackupex still
    supports all features and syntax as 2.2 version did, but is now
    deprecated and will be removed in next major release.

[xtrabackup](xtrabackup_bin/xtrabackup_binary.md)

    a compiled *C* binary that provides functionality to backup a whole *MySQL*
    database instance with *MyISAM*, *InnoDB*, and XtraDB tables.

[xbcrypt](xbcrypt/xbcrypt.md)

    utility used for encrypting and decrypting backup files.

[xbstream](xbstream/xbstream.md)

    utility that allows streaming and extracting files to/from the
    xbstream format.

[xbcloud](xbcloud/xbcloud.md)

    utility used for downloading and uploading full or part of xbstream
    archive from/to cloud.

After *Percona XtraBackup* 2.3 release the recommend way to take the backup is
using the *xtrabackup* script. More information on script options can be found
in [how to use xtrabackup](xtrabackup_bin/xtrabackup_binary.md).
