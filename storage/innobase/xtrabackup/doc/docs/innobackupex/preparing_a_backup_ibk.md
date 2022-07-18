# Preparing a Full Backup with *innobackupex*

The purpose of the **prepare stage** is to perform any pending operations and make the data consistent. After creating a backup, for example, uncommitted transactions must be undone or log transactions must be replayed. After this stage has finished, the data is ready.

To prepare a backup with *innobackupex* you have to use the
`innobackupex --apply-log` option and full path to the backup directory as an
argument:

```default
$ innobackupex --apply-log /path/to/BACKUP-DIR
```

and check the last line of the output for a confirmation on the process:

```default
150806 01:01:57  InnoDB: Shutdown completed; log sequence number 1609228
150806 01:01:57  innobackupex: completed OK!
```

If it succeeded, *innobackupex* performed all operations needed, leaving the
data ready to use immediately.

## Under the hood

*innobackupex* started the prepare process by reading the configuration from the
`backup-my.cnf` file in the backup directory.

After that, *innobackupex* replayed the committed transactions in the log files
(some transactions could have been done while the backup was being done) and
rolled back the uncommitted ones. Once this is done, all the information lay in
the tablespace (the InnoDB files), and the log files are re-created.

This implies calling `innobackupex --apply-log` twice. More details of
this process are shown in the [xtrabackup section](../xtrabackup_bin/xtrabackup_binary.md#xtrabackup-binary).

Note that this preparation is not suited for incremental backups. If you perform
it on the base of an incremental backup, you will not be able to “add” the
increments. See [Incremental Backups with innobackupex](incremental_backups_innobackupex.md).

## Other options to consider

### `innobackupex --use-memory`

The preparing process can be sped up by using more memory in it. It depends on
the free or available `RAM` on your system, it defaults to `100MB`. In
general, the more memory available to the process, the better. The amount of
memory used in the process can be specified by multiples of bytes:

```default
$ innobackupex --apply-log --use-memory=4G /path/to/BACKUP-DIR
```
