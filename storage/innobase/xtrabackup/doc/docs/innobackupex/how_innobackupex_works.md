# How innobackupex Works

From *Percona XtraBackup* version 2.3 `innobackupex` is has been
rewritten in *C* and set up as a symlink to the
`xtrabackup`. innobackupex supports all features and syntax as 2.2
version did, but it is now deprecated and will be removed in next major
release. Syntax for new features will not be added to the innobackupex, only to
the xtrabackup.

The following describes the rationale behind `innobackupex` actions.

## Making a Backup

If no mode is specified, innobackupex will assume the backup mode.

By default, it runs `xtrabackup` and lets it copy the
InnoDB data files. When `xtrabackup` finishes that,
innobackupex sees it create the `xtrabackup_suspended_2` file
and executes `FLUSH TABLES WITH READ LOCK`. `xtrabackup` will use
[Backup locks](https://www.percona.com/doc/percona-server/5.7/management/backup_locks.html#backup-locks)
where available as a lightweight alternative to `FLUSH TABLES WITH READ
LOCK`. This feature is available in *Percona Server* 5.6+. *Percona XtraBackup*
uses this automatically to copy non-InnoDB data to avoid blocking DML queries
that modify InnoDB tables. Then it begins copying the rest of the files.

*innobackupex* will then check *MySQL* variables to determine which features are
supported by server. Special interest are backup locks, changed page bitmaps,
GTID mode, etc. If everything goes well, the binary is started as a child
process.

*innobackupex* will wait for replicas in a replication setup if the option
`innobackupex --safe-slave-backup` is set and will flush all tables with
**READ LOCK**, preventing all *MyISAM* tables from writing (unless option
`innobackupex --no-lock` is specified).

!!! note

    Locking is done only for MyISAM and other non-InnoDB tables, and only **after** *Percona XtraBackup* is finished backing up all InnoDB/XtraDB data and logs. *Percona XtraBackup* will use `backup locks` where available as a lightweight alternative to `FLUSH TABLES WITH READ LOCK`. This feature is available in *Percona Server for MySQL* 5.6+. *Percona XtraBackup* uses this automatically to copy non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.

Once this is done, the backup of the files will begin. It will backup
`.frm`, `.MRG`, `.MYD`, `.MYI`, `.TRG`,
`.TRN`, `.ARM`, `.ARZ`, `.CSM`, `.CSV`, `.par`,
and `.opt` files.

When all the files are backed up, it resumes `ibbackup` and wait until
it finishes copying the transactions done while the backup was done. Then, the
tables are unlocked, the replica is started (if the option `innobackupex --safe-slave-backup`
was used) and the connection with the server is
closed. Then, it removes the `xtrabackup_suspended_2` file and permits `xtrabackup` to exit.

It will also create the following files in the directory of the backup:

`xtrabackup_checkpoints`

    containing the `LSN` and the type of backup;

`xtrabackup_binlog_info`

    containing the position of the binary log at the moment of backing up;

`xtrabackup_binlog_pos_innodb`

    containing the position of the binary log at the moment of backing up relative to *InnoDB* transactions;

`xtrabackup_slave_info`

    containing the MySQL binlog position of the source server in a replication setup via `SHOW SLAVE STATUS` if the innobackupex –slave-info option is passed;

`backup-my.cnf`

    containing only the my.cnf options required for the backup. For example, innodb_data_file_path, innodb_log_files_in_group, innodb_log_file_size, innodb_fast_checksum, innodb_page_size, innodb_log_block_size;

`xtrabackup_binary`

    containing the binary used for the backup;

`mysql-stderr`

    containing the `STDERR` of mysqld during the process and

`mysql-stdout`

    containing the `STDOUT` of the server.

Finally, the binary log position will be printed to `STDERR` and *innobackupex* will exit returning 0 if all went OK.

Note that the `STDERR` of *innobackupex* is not written in any file. You will have to redirect it to a file, e.g., `innobackupex OPTIONS 2> backupout.log`.

## Restoring a backup

To restore a backup with *innobackupex* the `innobackupex --copy-back` option must be used.

*innobackupex* will read from the `my.cnf` the variables `datadir`,
`innodb_data_home_dir`, `innodb_data_file_path`,
`innodb_log_group_home_dir` and check that the directories exist.

It will copy the *MyISAM* tables, indexes, etc. (`.frm`, `.MRG`,
`.MYD`, `.MYI`, `.TRG`, `.TRN`, `.ARM`,
`.ARZ`, `.CSM`, `.CSV`, `par` and `.opt` files) first,
*InnoDB* tables and indexes next and the log files at last. It will preserve
file’s attributes when copying them, you may have to change the files’ ownership
to `mysql` before starting the database server, as they will be owned by the
user who created the backup.

Alternatively, the `innobackupex --move-back` option may be used to restore a
backup. This option is similar to `innobackupex --copy-back` with the only
difference that instead of copying files it moves them to their target
locations. As this option removes backup files, it must be used with
caution. It is useful in cases when there is not enough free disk space
to hold both data files and their backup copies.
