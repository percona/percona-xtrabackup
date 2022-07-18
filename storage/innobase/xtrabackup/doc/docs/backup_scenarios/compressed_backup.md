# Compressed Backup

*Percona XtraBackup* supports compressed backups: a local or streaming backup
can be compressed or decompressed with xbstream.

## Creating Compressed Backups

In order to make a compressed backup you’ll need to use the xtrabackup –compress
option:

```bash
$ xtrabackup --backup --compress --target-dir=/data/compressed/
```

The `xtrabackup --compress` uses the `qpress` tool that you can install via
the `percona-release` package configuration tool as follows:

```bash
$ sudo percona-release enable tools
$ sudo apt update
$ sudo apt install qpress
```

!!! note

    Enable the repository: `percona-release enable-only tools release`. If you intend to use Percona XtraBackup in combination with the upstream MySQL Server, you only need to enable the `tools` repository: `percona-release enable-only tools`.

If you want to speed up the compression you can use the parallel compression,
which can be enabled with xtrabackup –compress-threads option.
Following example will use four threads for compression:

```bash
$ xtrabackup --backup --compress --compress-threads=4 \
--target-dir=/data/compressed/
```

Output should look like this

```default
...
170223 13:00:38 [01] Compressing ./test/sbtest1.frm to /tmp/compressed/test/sbtest1.frm.qp
170223 13:00:38 [01]        ...done
170223 13:00:38 [01] Compressing ./test/sbtest2.frm to /tmp/compressed/test/sbtest2.frm.qp
170223 13:00:38 [01]        ...done
...
170223 13:00:39 [00] Compressing xtrabackup_info
170223 13:00:39 [00]        ...done
xtrabackup: Transaction log of lsn (9291934) to (9291934) was copied.
170223 13:00:39 completed OK!
```

### Preparing the backup

Before you can prepare the backup you must uncompress all the files.
*Percona XtraBackup* has implemented `xtrabackup --decompress` option
that can be used to decompress the backup.

```bash
$ xtrabackup --decompress --target-dir=/data/compressed/
```

!!! note

    `xtrabackup --parallel` can be used with `xtrabackup --decompress` option to decompress multiple files simultaneously.

*Percona XtraBackup* does not automatically remove the compressed files. In
order to clean up the backup directory, use the
`xtrabackup --remove-original` option. If the files not removed
they are not copied or moved to the datadir if
`xtrabackup --copy-back` or `xtrabackup --move-back` are used.

When the files are uncompressed you can prepare the backup with the
`xtrabackup --prepare` option:

```bash
$ xtrabackup --prepare --target-dir=/data/compressed/
```

Check for a confirmation message:

```default
InnoDB: Starting shutdown...
InnoDB: Shutdown completed; log sequence number 9293846
170223 13:39:31 completed OK!
```

Now the files in `/data/compressed/` are ready to be used by the server.

### Restoring the backup

xtrabackup has a `xtrabackup --copy-back` option, which performs the
restoration of a backup to the server’s datadir:

```bash
$ xtrabackup --copy-back --target-dir=/data/backups/
```

The option copies all the data-related files back to the server’s `datadir`,
determined by the server’s `my.cnf` configuration file. Check the last line of the output for a success message:

```default
170223 13:49:13 completed OK!
```

Verify the file permissions after copying the data back. You may need
to adjust the permissions. For example, the following command changes the owner of the file location:

```bash
$ chown -R mysql:mysql /var/lib/mysql
```

Now that the `datadir` contains the restored data. You are ready to start
the server.
