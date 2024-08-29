# How Percona XtraBackup Works

*Percona XtraBackup* is based on InnoDB’s crash-recovery functionality.
It copies your *InnoDB* data files, which results in data that is internally
inconsistent; but then it performs crash recovery on the files to make them a
consistent, usable database again.

This works because *InnoDB* maintains a redo log, also called the transaction
log. This contains a record of every change to InnoDB data. When *InnoDB*
starts, it inspects the data files and the transaction log, and performs two
steps. It applies committed transaction log entries to the data files, and it
performs an undo operation on any transactions that modified data but did not
commit.

*Percona XtraBackup* works by remembering the log sequence number (LSN)
when it starts, and then copying away the data files. It takes some time to do
this, so if the files are changing, then they reflect the state of the database
at different points in time. At the same time, *Percona XtraBackup* runs a
background process that watches the transaction log files, and copies changes
from it. *Percona XtraBackup* needs to do this continually because the
transaction logs are written in a round-robin fashion, and can be reused after a
while. *Percona XtraBackup* needs the transaction log records for every change
to the data files since it began execution.

*Percona XtraBackup* uses [Backup locks](https://docs.percona.com/percona-server/8.0/management/backup_locks.html)
where available as a lightweight alternative to `FLUSH TABLES WITH READ
LOCK`. This feature is available in *Percona Server for MySQL* 5.6+. *MySQL* 8.0 allows
acquiring an instance level backup lock via the `LOCK INSTANCE FOR BACKUP`
statement.

Locking is only done for *MyISAM* and other non-InnoDB tables
**after** *Percona XtraBackup* finishes backing up all InnoDB/XtraDB data and
logs. *Percona XtraBackup* uses this automatically to copy non-InnoDB data to
avoid blocking DML queries that modify *InnoDB* tables.

**_Important_** - The `BACKUP_ADMIN` privilege is required to query the 
`performance_schema_log_status` for either `LOCK 
INSTANCE FOR 
BACKUP` or `LOCK TABLES FOR BACKUP`.

*xtrabackup* tries to avoid backup locks and `FLUSH TABLES WITH READ LOCK`
when the instance contains only InnoDB tables. In this case, *xtrabackup*
obtains binary log coordinates from `performance_schema.log_status`. `FLUSH
TABLES WITH READ LOCK` is still required in MySQL 8.0 when xtrabackup is
started with the `--slave-info`. The `log_status` table in Percona
Server for MySQL 8.0 is extended to include the relay log coordinates, so no locks are
needed even with the `--slave-info` option.

**_See also_** [*MySQL* Documentation: LOCK INSTANCE FOR BACKUP](https://dev.mysql.com/doc/refman/8.0/en/lock-instance-for-backup.html)

When backup locks are supported by the server, *xtrabackup* first copies
*InnoDB* data, runs the `LOCK TABLES FOR BACKUP` and then copies the *MyISAM*
tables. Once this is done, the backup of the files will
begin. It will backup .frm, .MRG, .MYD, .MYI, .CSM,
.CSV, `.sdi` and `.par` files.

After that *xtrabackup* will use `LOCK BINLOG FOR BACKUP` to block all
operations that might change either binary log position or
`Exec_Master_Log_Pos` or `Exec_Gtid_Set` (i.e. source binary log coordinates
corresponding to the current SQL thread state on a replication replica) as
reported by `SHOW MASTER/SLAVE STATUS`. *xtrabackup* will then finish copying
the REDO log files and fetch the binary log coordinates. After this is completed
*xtrabackup* will unlock the binary log and tables.

Finally, the binary log position will be printed to `STDERR` and *xtrabackup*
will exit returning 0 if all went OK.

Note that the `STDERR` of *xtrabackup* is not written in any file. You will
have to redirect it to a file, e.g., `xtrabackup OPTIONS 2> backupout.log`.

It will also create the following files in the
directory of the backup.

During the `prepare` phase, *Percona XtraBackup* performs crash recovery against
the copied data files, using the copied transaction log file. After this is
done, the database is ready to restore and use.

The backed-up *MyISAM* and *InnoDB* tables will be eventually consistent with
each other, because after the prepare (recovery) process, *InnoDB*’s data is
rolled forward to the point at which the backup completed, not rolled back to
the point at which it started. This point in time matches where the `FLUSH
TABLES WITH READ LOCK` was taken, so the *MyISAM* data and the prepared
*InnoDB* data are in sync.

The *xtrabackup* offers many features not mentioned in the preceding
explanation. The functionality of each tool is explained in more
detail further in this manual. In brief, though, the tools enable you
to do operations such as streaming and incremental backups with
various combinations of copying the data files, copying the log files,
and applying the logs to the data.

## Restoring a backup

To restore a backup with *xtrabackup* you can use the `--copy-back` or
`--move-back` options.

*xtrabackup* will read from the `my.cnf` the variables datadir,
innodb_data_home_dir, innodb_data_file_path,
innodb_log_group_home_dir and check that the directories exist.

It will copy the *MyISAM* tables, indexes, etc. (.MRG, .MYD,
.MYI, .CSM, .CSV, `.sdi`,
and `par` files) first, *InnoDB* tables and indexes next and the log files at
last. It will preserve file’s attributes when copying them, you may have to
change the files’ ownership to `mysql` before starting the database server, as
they will be owned by the user who created the backup.

Alternatively, the `--move-back` option may be used to
restore a backup. This option is similar to `--copy-back`
with the only difference that instead of copying files it moves them to their
target locations. As this option removes backup files, it must be used with
caution. It is useful in cases when there is not enough free disk space to hold
both data files and their backup copies.
