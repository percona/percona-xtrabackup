# The innobackupex Option Reference

This page documents all of the command-line options for the `innobackupex`.

## Options


### --apply-log
Prepare a backup in `BACKUP-DIR` by applying the transaction log file named
`xtrabackup_logfile` located in the same directory. Also, create new
transaction logs. The InnoDB configuration is read from the file
`backup-my.cnf` created by *innobackupex* when the backup was
made. `innobackupex --apply-log` uses InnoDB configuration from
`backup-my.cnf` by default, or from `--defaults-file`, if specified. InnoDB
configuration in this context means server variables that affect data format,
i.e. `innodb_page_size`, `innodb_log_block_size`, etc. Location-related
variables, like `innodb_log_group_home_dir` or `innodb_data_file_path` are
always ignored by `--apply-log`, so preparing a backup always works with data
files from the backup directory, rather than any external ones.


### --backup-locks
This option controls if backup locks should be used instead of `FLUSH TABLES
WITH READ LOCK` on the backup stage. The option has no effect when backup
locks are not supported by the server. This option is enabled by default,
disable with `--no-backup-locks`.


### --no-backup-locks
Explicity disables the option `–-backup-locks` which is enabled by
default.


### --close-files
Do not keep files opened. This option is passed directly to xtrabackup. When
xtrabackup opens tablespace it normally doesn’t close its file handle in
order to handle the DDL operations correctly. However, if the number of
tablespaces is really huge and can not fit into any limit, there is an option
to close file handles once they are no longer accessed. *Percona XtraBackup*
can produce inconsistent backups with this option enabled. Use at your own
risk.


### --compress
This option instructs xtrabackup to compress backup copies of InnoDB data
files. It is passed directly to the xtrabackup child process. See the
xtrabackup [documentation](../xtrabackup_bin/xtrabackup_binary.md) for details.


### --compress-threads=#
This option specifies the number of worker threads that will be used for
parallel compression. It is passed directly to the xtrabackup child
process. See the xtrabackup [documentation](../xtrabackup_bin/xtrabackup_binary.md) for details.


### --compress-chunk-size=#
This option specifies the size of the internal working buffer for each
compression thread, measured in bytes. It is passed directly to the
xtrabackup child process. The default value is 64K. See the
`xtrabackup` [documentation](../xtrabackup_bin/xtrabackup_binary.md) for details.


### --copy-back
Copy all the files in a previously made backup from the backup directory to
their original locations. *Percona XtraBackup* `innobackupex --copy-back`
option will not copy over existing files unless
`innobackupex --force-non-empty-directories` option is specified.


### --databases=LIST
This option specifies the list of databases that *innobackupex* should back
up. The option accepts a string argument or path to file that contains the
list of databases to back up. The list is of the form
“databasename1[.table_name1] databasename2[.table_name2] …”. If this
option is not specified, all databases containing *MyISAM* and *InnoDB*
tables will be backed up. Please make sure that --databases contains all of
the *InnoDB* databases and tables, so that all of the innodb.frm files are
also backed up. In case the list is very long, this can be specified in a
file, and the full path of the file can be specified instead of the
list. (See option --tables-file.)


### --decompress
Decompresses all files with the .qp extension in a backup previously made
with the `innobackupex --compress` option. The `innobackupex --parallel` option will allow multiple files to be decrypted and/or
decompressed simultaneously. In order to decompress, the qpress utility MUST
be installed and accessible within the path. *Percona XtraBackup* doesn’t
automatically remove the compressed files. In order to clean up the backup
directory users should remove the `\*.qp` files manually.


### --decrypt=ENCRYPTION-ALGORITHM
Decrypts all files with the .xbcrypt extension in a backup previously made
with --encrypt option. The `innobackupex --parallel` option will
allow multiple files to be decrypted and/or decompressed simultaneously.


### --defaults-file=[MY.CNF]
This option accepts a string argument that specifies what file to read the
default MySQL options from. Must be given as the first option on the
command-line.


### --defaults-extra-file=[MY.CNF]
This option specifies what extra file to read the default *MySQL* options
from before the standard defaults-file. Must be given as the first option on
the command-line.


### --defaults-group=GROUP-NAME
This option accepts a string argument that specifies the group which should
be read from the configuration file. This is needed if you use
mysqld_multi. This can also be used to indicate groups other than mysqld and
xtrabackup.


### --encrypt=ENCRYPTION_ALGORITHM
This option instructs xtrabackup to encrypt backup copies of InnoDB data
files using the algorithm specified in the ENCRYPTION_ALGORITHM. It is passed
directly to the xtrabackup child process. See the `xtrabackup`
[documentation](../xtrabackup_bin/xtrabackup_binary.md) for more details.

Currently, the following algorithms are supported: `AES128`,
`AES192` and `AES256`.


### --encrypt-key=ENCRYPTION_KEY
This option instructs xtrabackup to use the given proper length encryption
key as the ENCRYPTION_KEY when using the --encrypt option. It is passed
directly to the xtrabackup child process. See the `xtrabackup`
[documentation](../xtrabackup_bin/xtrabackup_binary.md) for more details.

It is not recommended to use this option where there is uncontrolled access
to the machine as the command line and thus the key can be viewed as part of
the process info.


### --encrypt-key-file=ENCRYPTION_KEY_FILE
This option instructs xtrabackup to use the encryption key stored in the
given ENCRYPTION_KEY_FILE when using the –encrypt option. It is passed
directly to the xtrabackup child process. See the `xtrabackup`
[documentation](../xtrabackup_bin/xtrabackup_binary.md) for more details.

The file must be a simple binary (or text) file that contains exactly the key
to be used.


### --encrypt-threads=#
This option specifies the number of worker threads that will be used for
parallel encryption. It is passed directly to the xtrabackup child
process. See the `xtrabackup` [documentation](../xtrabackup_bin/xtrabackup_binary.md) for more details.


### --encrypt-chunk-size=#
This option specifies the size of the internal working buffer for each
encryption thread, measured in bytes. It is passed directly to the xtrabackup
child process. See the `xtrabackup` [documentation](../xtrabackup_bin/xtrabackup_binary.md) for more details.


### --export
This option is passed directly to `xtrabackup --export` option. It
enables exporting individual tables for import into another server. See the
*xtrabackup* documentation for details.


### --extra-lsndir=DIRECTORY
This option accepts a string argument that specifies the directory in which
to save an extra copy of the `xtrabackup_checkpoints` file. It is
passed directly to *xtrabackup*’s `innobackupex --extra-lsndir` option. See the xtrabackup documentation for details.


### --force-non-empty-directories
When specified, it makes `innobackupex --copy-back` option or
`innobackupex --move-back` option transfer files to non-empty
directories. No existing files will be overwritten. If --copy-back
or --move-back has to copy a file from the backup directory which already
exists in the destination directory, it will still fail with an error.


### --galera-info
This options creates the `xtrabackup_galera_info` file which contains the
local node state at the time of the backup. Option should be used when
performing the backup of Percona-XtraDB-Cluster. Has no effect when backup
locks are used to create the backup.


### --help
This option displays a help screen and exits.


### --history=NAME
This option enables the tracking of backup history in the
`PERCONA_SCHEMA.xtrabackup_history` table. An optional history series name
may be specified that will be placed with the history record for the current
backup being taken.


### --host=HOST
This option accepts a string argument that specifies the host to use when
connecting to the database server with TCP/IP. It is passed to the mysql
child process without alteration. See `mysql --help` for details.


### --ibbackup=IBBACKUP-BINARY
This option specifies which *xtrabackup* binary should be used. The option
accepts a string argument. IBBACKUP-BINARY should be the command used to run
*Percona XtraBackup*. The option can be useful if the *xtrabackup* binary is
not in your search path or working directory. If this option is not
specified, *innobackupex* attempts to determine the binary to use
automatically.


### --include=REGEXP
This option is a regular expression to be matched against table names in
`databasename.tablename` format. It is passed directly to xtrabackup’s
`xtrabackup --tables` option. See the xtrabackup
documentation for details.


### --incremental
This option tells *xtrabackup* to create an incremental backup, rather than a
full one. It is passed to the *xtrabackup* child process. When this option is
specified, either `innobackupex --incremental-lsn` or
`innobackupex --incremental-basedir` can also be given. If neither option is
given, option `innobackupex --incremental-basedir` is passed to
`xtrabackup` by default, set to the first timestamped backup
directory in the backup base directory.


### --incremental-basedir=DIRECTORY
This option accepts a string argument that specifies the directory containing
the full backup that is the base dataset for the incremental backup. It is
used with the `innobackupex --incremental` option.


### --incremental-dir=DIRECTORY
This option accepts a string argument that specifies the directory where the
incremental backup will be combined with the full backup to make a new full
backup. It is used with the `innobackupex --incremental` option.


### --incremental-history-name=NAME
This option specifies the name of the backup series stored in the
[PERCONA_SCHEMA.xtrabackup_history](storing_history.md#xtrabackup-history) history record
to base an incremental backup on. Percona Xtrabackup will search the history
table looking for the most recent (highest innodb_to_lsn), successful backup
in the series and take the to_lsn value to use as the starting lsn for the
incremental backup. This will be mutually exclusive with
`innobackupex --incremental-history-uuid`, `innobackupex --incremental-basedir`
and `innobackupex --incremental-lsn`. If no
valid lsn can be found (no series by that name, no successful backups by that
name) xtrabackup will return with an error. It is used with the
`innobackupex --incremental` option.


### --incremental-history-uuid=UUID
This option specifies the UUID of the specific history record stored in the
[PERCONA_SCHEMA.xtrabackup_history](storing_history.md#xtrabackup-history) to base an
incremental backup on. `innobackupex --incremental-history-name`, `innobackupex --incremental-basedir\` and
`innobackupex --incremental-lsn`. If no valid lsn can be found (no
success record with that uuid) xtrabackup will return with an error. It is
used with the `innobackupex --incremental` option.


### --incremental-lsn=LSN
This option accepts a string argument that specifies the log sequence number
(`LSN`) to use for the incremental backup. It is used with the
`innobackupex --incremental` option. It is used instead of specifying
`innobackupex --incremental-basedir`. For databases created by *MySQL* and
*Percona Server* 5.0-series versions, specify the as two 32-bit integers in
high:low format. For databases created in 5.1 and later, specify the LSN as a
single 64-bit integer.


### --kill-long-queries-timeout=SECONDS
This option specifies the number of seconds innobackupex waits between
starting `FLUSH TABLES WITH READ LOCK` and killing those queries that block
it. Default is 0 seconds, which means innobackupex will not attempt to kill
any queries. In order to use this option xtrabackup user should have
`PROCESS` and `SUPER` privileges. Where supported (Percona Server 5.6+)
xtrabackup will automatically use [Backup Locks](https://www.percona.com/doc/percona-server/5.6/management/backup_locks.html#backup-locks)
as a lightweight alternative to `FLUSH TABLES WITH READ LOCK` to copy
non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.


### --kill-long-query-type=all|select
This option specifies which types of queries should be killed to unblock the
global lock. Default is “all”.


### --ftwrl-wait-timeout=SECONDS
This option specifies time in seconds that innobackupex should wait for
queries that would block `FLUSH TABLES WITH READ LOCK` before running
it. If there are still such queries when the timeout expires, innobackupex
terminates with an error. Default is 0, in which case innobackupex does not
wait for queries to complete and starts `FLUSH TABLES WITH READ LOCK`
immediately. Where supported (Percona Server 5.6+) xtrabackup will
automatically use [Backup Locks](https://www.percona.com/doc/percona-server/5.6/management/backup_locks.html#backup-locks)
as a lightweight alternative to `FLUSH TABLES WITH READ LOCK` to copy
non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.


### --ftwrl-wait-threshold=SECONDS
This option specifies the query run time threshold which is used by
innobackupex to detect long-running queries with a non-zero value of
`innobackupex –ftwrl-wait-timeout`. `FLUSH TABLES WITH READ LOCK`
is not started until such long-running queries exist. This option has no
effect if –ftwrl-wait-timeout is 0. Default value is 60 seconds. Where
supported (Percona Server 5.6+) xtrabackup will automatically use [Backup
Locks](https://www.percona.com/doc/percona-server/5.6/management/backup_locks.html#backup-locks)
as a lightweight alternative to `FLUSH TABLES WITH READ LOCK` to copy
non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.


### --ftwrl-wait-query-type=all|update
This option specifies which types of queries are allowed to complete before
innobackupex will issue the global lock. Default is all.


### --log-copy-interval=#
This option specifies time interval between checks done by log copying thread
in milliseconds.


### --move-back
Move all the files in a previously made backup from the backup directory to
their original locations. As this option removes backup files, it must be
used with caution.


### --no-lock
Use this option to disable table lock with `FLUSH TABLES WITH READ
LOCK`. Use it only if ALL your tables are InnoDB and you **DO NOT CARE**
about the binary log position of the backup. This option shouldn’t be used if
there are any `DDL` statements being executed or if any updates are
happening on non-InnoDB tables (this includes the system MyISAM tables in the
*mysql* database), otherwise it could lead to an inconsistent backup. Where
supported (Percona Server 5.6+) xtrabackup will automatically use [Backup
Locks](https://www.percona.com/doc/percona-server/5.6/management/backup_locks.html#backup-locks)
as a lightweight alternative to `FLUSH TABLES WITH READ LOCK` to copy
non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.  If
you are considering to use `innobackupex --no-lock because` your backups are
failing to acquire the lock, this could be because of incoming replication
events preventing the lock from succeeding. Please try using
`innobackupex --safe-slave-backup` to momentarily stop the replication replica
thread, this may help the backup to succeed and you then don’t need to resort
to using this option.  `xtrabackup_binlog_info` is not created
when –no-lock option is used (because `SHOW MASTER STATUS` may be
inconsistent), but under certain conditions
`xtrabackup_binlog_pos_innodb` can be used instead to get consistent
binlog coordinates as described in [Working with Binary Logs](../xtrabackup_bin/working_with_binary_logs.md#working-with-binlogs).


### --no-timestamp
This option prevents creation of a time-stamped subdirectory of the
`BACKUP-ROOT-DIR` given on the command line. When it is specified, the
backup is done in `BACKUP-ROOT-DIR` instead.


### --no-version-check
This option disables the version check. If you do not pass this option, the
automatic version check is enabled implicitly when `xtrabackup` runs in the `--backup` mode. To disable the version check, explicitly pass
the `--no-version-check` option when invoking `xtrabackup`. 
When the automatic version check is enabled,|program| performs a version check against the server on the backup stage after creating a server
connection. 

`xtrabackup` sends the following information to the server:

* MySQL flavour and version

* Operating system name

* Percona Toolkit version

* Perl version

Each piece of information has a unique identifier which is an MD5 hash value
that Percona Toolkit uses to obtain statistics about how it is used. This value is
a random UUID; no client information is either collected or stored.

### --parallel=NUMBER-OF-THREADS
This option accepts an integer argument that specifies the number of threads
the `xtrabackup` child process should use to back up files
concurrently.  Note that this option works on file level, that is, if you
have several .ibd files, they will be copied in parallel. If your tables are
stored together in a single tablespace file, it will have no effect. This
option will allow multiple files to be decrypted and/or decompressed
simultaneously. In order to decompress, the qpress utility MUST be installed
and accessable within the path. This process will remove the original
compressed/encrypted files and leave the results in the same location. It is
passed directly to xtrabackup’s `xtrabackup --parallel` option. See
the `xtrabackup` documentation for details


### --password=PASSWORD
This option accepts a string argument specifying the password to use when
connecting to the database. It is passed to the `mysql` child
process without alteration. See `mysql --help` for details.


### --port=PORT
This option accepts a string argument that specifies the port to use when
connecting to the database server with TCP/IP. It is passed to the
`mysql` child process. It is passed to the `mysql` child
process without alteration. See `mysql --help` for details.


### --rebuild-indexes
This option only has effect when used together with the `--apply-log <innobackupex --apply-log>`
option and is passed directly to xtrabackup. When used, makes xtrabackup
rebuild all secondary indexes after applying the log. This option is normally
used to prepare compact backups. See the `xtrabackup` documentation
for more information.


### --rebuild-threads=NUMBER-OF-THREADS
This option only has effect when used together with the `innobackupex --apply-log`
and `innobackupex --rebuild-indexes` option and is passed directly to
xtrabackup. When used, xtrabackup processes tablespaces in parallel with the
specified number of threads when rebuilding indexes. See the
`xtrabackup` documentation for more information.


### --redo-only
This option should be used when preparing the base full backup and when
merging all incrementals except the last one. It is passed directly to
xtrabackup’s `xtrabackup --apply-log-only` option. This forces
`xtrabackup` to skip the “rollback” phase and do a “redo” only. This
is necessary if the backup will have incremental changes applied to it
later. See the *xtrabackup* [documentation](../xtrabackup_bin/incremental_backups.md) for details.


### --rsync
Uses the `rsync` utility to optimize local file transfers. When this
option is specified, `innobackupex` uses `rsync` to copy
all non-InnoDB files instead of spawning a separate `cp` for each
file, which can be much faster for servers with a large number of databases
or tables.  This option cannot be used together with `innobackupex --stream`.


### --safe-slave-backup
When specified, innobackupex will stop the replica SQL thread just before
running `FLUSH TABLES WITH READ LOCK` and wait to start backup until
`Slave_open_temp_tables` in `SHOW STATUS` is zero. If there are no open
temporary tables, the backup will take place, otherwise the SQL thread will
be started and stopped until there are no open temporary tables. The backup
will fail if `Slave_open_temp_tables` does not become zero after
`innobackupex --safe-slave-backup-timeout` seconds. The replica SQL
thread will be restarted when the backup finishes.


### --safe-slave-backup-timeout=SECONDS
How many seconds `innobackupex --safe-slave-backup` should wait for
`Slave_open_temp_tables` to become zero. Defaults to 300 seconds.


### --slave-info
This option is useful when backing up a replication replica server. It prints
the binary log position and name of the source server. It also writes this
information to the `xtrabackup_slave_info` file as a `CHANGE MASTER`
command. A new replica for this source can be set up by starting a replica server
on this backup and issuing a `CHANGE MASTER` command with the binary log
position saved in the `xtrabackup_slave_info` file.


### --socket
This option accepts a string argument that specifies the socket to use when
connecting to the local database server with a UNIX domain socket. It is
passed to the mysql child process without alteration. See `mysql --help` for details.


### --stream=STREAMNAME
This option accepts a string argument that specifies the format in which to
do the streamed backup. The backup will be done to `STDOUT` in the
specified format. Currently, supported formats are `tar` and `xbstream`. Uses
[xbstream](../xbstream/xbstream.md), which is available in *Percona XtraBackup*
distributions. If you specify a path after this option, it will be interpreted
as the value of `tmpdir`


### --tables-file=FILE
This option accepts a string argument that specifies the file in which there
are a list of names of the form `database.table`, one per line. The option
is passed directly to `xtrabackup` ‘s `innobackupex --tables-file`
option.


### --throttle=#
This option limits the number of chunks copied per second. The chunk size is
*10 MB*. To limit the bandwidth to *10 MB/s*, set the option to *1*:
`--throttle=1`.

!!! seealso

    More information about how to throttle a backup [Throttling Backups](../advanced/throttling_backups.md#throttling-backups).

### --tmpdir=DIRECTORY
This option accepts a string argument that specifies the location where a
temporary file will be stored. It may be used when `innobackupex --stream` is
specified. For these options, the transaction log will first be stored to a
temporary file, before streaming or copying to a remote host. This option
specifies the location where that temporary file will be stored. If the
option is not specified, the default is to use the value of `tmpdir` read
from the server configuration. innobackupex is passing the tmpdir value
specified in my.cnf as the `--target-dir` option to the xtrabackup binary. Both
[mysqld] and [xtrabackup] groups are read from my.cnf. If there is tmpdir in
both, then the value being used depends on the order of those group in
my.cnf.


### --use-memory=#
This option accepts a string argument that specifies the amount of memory in
bytes for `xtrabackup` to use for crash recovery while preparing a
backup. Multiples are supported providing the unit (e.g. 1MB, 1M, 1GB,
1G). It is used only with the option `innobackupex --apply-log`. It is passed
directly to xtrabackup’s `xtrabackup --use-memory` option. See the
*xtrabackup* documentation for details.


### --user=USER
This option accepts a string argument that specifies the user (i.e., the
*MySQL* username used when connecting to the server) to login as, if that’s
not the current user. It is passed to the mysql child process without
alteration. See `mysql --help` for details.


### --version
This option displays the *innobackupex* version and copyright notice and then
exits.
