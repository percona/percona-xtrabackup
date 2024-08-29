# The xtrabackup Option Reference

This page documents all of the command-line options for the
**xtrabackup** binary.

## Modes of operation

You invoke *xtrabackup* in one of the following modes:


* `--backup` mode to make a backup in a target directory


* `--prepare` mode to restore data from a backup (created in `--backup` mode)


* `--copy-back` to copy data from a backup to the location
that contained the original data; to move data instead of copying use
the alternate `--move-back` mode.


* `--stats` mode to scan the specified data files and print out index statistics.

When you intend to run *xtrabackup* in any of these modes, use the following syntax:

```
$ xtrabackup [--defaults-file=#] --backup|--prepare|--copy-back|--stats [OPTIONS]
```

For example, the `--prepare` mode is applied as follows:

```
$ xtrabackup --prepare --target-dir=/data/backup/mysql/
```

For all modes, the default options are read from the **xtrabackup** and
**mysqld** configuration groups from the following files in the given order:


1. `/etc/my.cnf`


2. `/etc/mysql/my.cnf`


3. `/usr/etc/my.cnf`


4. `~/.my.cnf`.

As the first parameter to *xtrabackup* (in place of the `--defaults-file`,
you may supply one of the following:


* `--print-defaults` to have *xtrabackup* print the argument list and exit.


* `--no-defaults` to forbid reading options from any file but the login file.


* `--defaults-file`  to read the default options from the given file.


* `--defaults-extra-file` to read the specified additional file after
the global files have been read.


* `--defaults-group-suffix` to read the configuration groups with the
given suffix. The effective group name is constructed by concatenating the default
configuration groups (**xtrabackup** and **mysqld**) with the given suffix.


* `--login-path` to read the given path from the login file.

### InnoDB Options

There is a large group of InnoDB options that are normally read from the
`my.cnf` configuration file, so that *xtrabackup* boots up its embedded
InnoDB in the same configuration as your current server. You normally do not
need to specify them explicitly. These options have the same behavior in InnoDB
and XtraDB. See `--innodb-miscellaneous` for more information.

## Options


### --apply-log-only()
This option causes only the redo stage to be performed when preparing a
backup. It is very important for incremental backups.


### --backup()
Make a backup and place it in `--target-dir`. See
Creating a backup.


### --backup-lock-timeout()
The timeout in seconds for attempts to acquire metadata locks.


### --backup-lock-retry-count()
The number of attempts to acquire metadata locks.


### --backup-locks()
This option controls if backup locks should be used instead of `FLUSH TABLES
WITH READ LOCK` on the backup stage. The option has no effect when backup
locks are not supported by the server. This option is enabled by default,
disable with `--no-backup-locks`.


### --check-privileges()
This option checks if *Percona XtraBackup* has all required privileges.
If a missing privilege is required for the current operation,
it will terminate and print out an error message.
If a missing privilege is not required for the current operation,
but may be necessary for some other XtraBackup operation,
the process is not aborted and a warning is printed.

```
xtrabackup: Error: missing required privilege LOCK TABLES on *.*
xtrabackup: Warning: missing required privilege REPLICATION CLIENT on *.*
```


### --close-files()
Do not keep files opened. When *xtrabackup* opens tablespace it normally
doesn’t close its file handle in order to handle the DDL operations
correctly. However, if the number of tablespaces is really huge and can not
fit into any limit, there is an option to close file handles once they are
no longer accessed. *Percona XtraBackup* can produce inconsistent backups
with this option enabled. Use at your own risk.


### --compress()
This option tells *xtrabackup* to compress all output data, including the
transaction log file and meta data files, using either the `quicklz` or
`lz4` compression algorithm. `quicklz` is chosen by default.

When using `--compress=quicklz` or `--compress`, the resulting files have
the qpress archive format, i.e. every `\*.qp` file produced by *xtrabackup* is
essentially a one-file qpress archive and can be extracted and uncompressed
by the [qpress](http://www.quicklz.com/) file archiver.

`--compress=lz4` produces `\*.lz4` files. You can extract the contents of
these files by using a program such as `lz4`.


### --compress-chunk-size(=#)
Size of working buffer(s) for compression threads in bytes. The default
value is 64K.


### --compress-threads(=#)
This option specifies the number of worker threads used by *xtrabackup* for
parallel data compression. This option defaults to `1`. Parallel
compression (`--compress-threads`) can be used together
with parallel file copying (`--parallel`). For example,
`--parallel=4 --compress --compress-threads=2` will create 4 I/O threads
that will read the data and pipe it to 2 compression threads.


### --copy-back()
Copy all the files in a previously made backup from the backup directory to
their original locations. This option will not copy over existing files
unless `--force-non-empty-directories` option is
specified.


### --core-file()
Write core on fatal signals.


### --databases(=#)
This option specifies a list of databases and tables that should be backed
up. The option accepts the list of the form `"databasename1[.table_name1]
databasename2[.table_name2] . . ."`.


### --databases-exclude(=name)
Excluding databases based on name, Operates the same way
as `--databases`, but matched names are excluded from
backup. Note that this option has a higher priority than
`--databases`.


### --databases-file(=#)
This option specifies the path to the file containing the list of databases
and tables that should be backed up. The file can contain the list elements
of the form `databasename1[.table_name1]`, one element per line.


### --datadir(=DIRECTORY)
The source directory for the backup. This should be the same as the datadir
for your *MySQL* server, so it should be read from `my.cnf` if that
exists; otherwise you must specify it on the command line.

When combined with the `--copy-back` or
`--move-back` option, `--datadir`
refers to the destination directory.

Once connected to the server, in order to perform a backup you will need
`READ` and `EXECUTE` permissions at a filesystem level in the
server’s datadir.


### --debug-sleep-before-unlock(=#)
This is a debug-only option used by the *xtrabackup* test suite.


### --debug-sync(=name)
The debug sync point. This option is only used by the *xtrabackup* test suite.


### --decompress()
Decompresses all files with the `.qp` extension in a backup previously
made with the `--compress` option. The
`--parallel` option will allow multiple files to be
decrypted simultaneously. In order to decompress, the qpress utility MUST be
installed and accessible within the path. *Percona XtraBackup* does not
automatically remove the compressed files. In order to clean up the backup
directory users should use `--remove-original` option.

The `--decompress` option may be used with *xbstream* to
decompress individual qpress files.

If you used the `lz4` compression algorithm to compress the files
(`--compress=lz4`), change the `--decompress` parameter
accordingly: `--decompress=lz4`.


### --decompress-threads(=#)
Force *xbstream* to use the specified number of threads for
decompressing.


### --decrypt(=ENCRYPTION-ALGORITHM)
Decrypts all files with the `.xbcrypt` extension in a backup
previously made with `--encrypt` option. The
`--parallel` option will allow multiple files to be
decrypted simultaneously. *Percona XtraBackup* doesn’t
automatically remove the encrypted files. In order to clean up the backup
directory users should use `--remove-original` option.


### --defaults-extra-file(=[MY.CNF])
Read this file after the global files are read. Must be given as the first
option on the command-line.


### --defaults-file(=[MY.CNF])
Only read default options from the given file. Must be given as the first
option on the command-line. Must be a real file; it cannot be a symbolic
link.


### --defaults-group(=GROUP-NAME)
This option is to set the group which should be read from the configuration
file. This is used by *xtrabackup* if you use the
`--defaults-group` option. It is needed for
`mysqld_multi` deployments.


### --defaults-group-suffix(=#)
Also reads groups with concat(group, suffix).


### --dump-innodb-buffer-pool()
This option controls whether or not a new dump of buffer pool
content should be done.

With `--dump-innodb-buffer-pool`, *xtrabackup*
makes a request to the server to start the buffer pool dump (it
takes some time to complete and is done in background) at the
beginning of a backup provided the status variable
`innodb_buffer_pool_dump_status` reports that the dump has been
completed.

```
$ xtrabackup --backup --dump-innodb-buffer-pool --target-dir=/home/user/backup
```

By default, this option is set to OFF.

If `innodb_buffer_pool_dump_status` reports that there is running
dump of buffer pool, *xtrabackup* waits for the dump to complete
using the value of `--dump-innodb-buffer-pool-timeout`

The file `ib_buffer_pool` stores tablespace ID and page ID
data used to warm up the buffer pool sooner.


### --dump-innodb-buffer-pool-timeout()
This option contains the number of seconds that *xtrabackup* should
monitor the value of `innodb_buffer_pool_dump_status` to
determine if buffer pool dump has completed.

This option is used in combination with
`--dump-innodb-buffer-pool`. By default, it is set to 10
seconds.


### --dump-innodb-buffer-pool-pct()
This option contains the percentage of the most recently used buffer pool
pages to dump.

This option is effective if `--dump-innodb-buffer-pool` option is set
to ON. If this option contains a value, *xtrabackup* sets the *MySQL*
system variable `innodb_buffer_pool_dump_pct`. As soon as the buffer pool
dump completes or it is stopped (see
`--dump-innodb-buffer-pool-timeout`), the value of the *MySQL* system
variable is restored.


### --encrypt(=ENCRYPTION_ALGORITHM)
This option instructs xtrabackup to encrypt backup copies of InnoDB data
files using the algorithm specified in the ENCRYPTION_ALGORITHM. Currently
supported algorithms are: `AES128`, `AES192` and `AES256`


### --encrypt-key(=ENCRYPTION_KEY)
A proper length encryption key to use. It is not recommended to use this
option where there is uncontrolled access to the machine as the command line
and thus the key can be viewed as part of the process info.


### --encrypt-key-file(=ENCRYPTION_KEY_FILE)
The name of a file where the raw key of the appropriate length can be read
from. The file must be a simple binary (or text) file that contains exactly
the key to be used.

It is passed directly to the xtrabackup child process. See the
**xtrabackup** documentation for more details.


### --encrypt-threads(=#)
This option specifies the number of worker threads that will be used for
parallel encryption/decryption.
See the **xtrabackup** documentation for more details.


### --encrypt-chunk-size(=#)
This option specifies the size of the internal working buffer for each
encryption thread, measured in bytes. It is passed directly to the
xtrabackup child process. See the **xtrabackup** documentation for more details.


### --export()
Create files necessary for exporting tables. See Restoring Individual
Tables.


### --extra-lsndir(=DIRECTORY)
(for –backup): save an extra copy of the `xtrabackup_checkpoints`
and `xtrabackup_info` files in this directory.


### --force-non-empty-directories()
When specified, it makes `--copy-back` and
`--move-back` option transfer files to non-empty
directories. No existing files will be overwritten. If files that need to
be copied/moved from the backup directory already exist in the destination
directory, it will still fail with an error.


### --ftwrl-wait-timeout(=SECONDS)
This option specifies time in seconds that xtrabackup should wait for
queries that would block `FLUSH TABLES WITH READ LOCK` before running it.
If there are still such queries when the timeout expires, xtrabackup
terminates with an error. Default is `0`, in which case it does not wait
for queries to complete and starts `FLUSH TABLES WITH READ LOCK`
immediately. Where supported *xtrabackup* will
automatically use [Backup Locks](https://www.percona.com/doc/percona-server/8.0/management/backup_locks.html#backup-locks)
as a lightweight alternative to `FLUSH TABLES WITH READ LOCK` to copy
non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.


### --ftwrl-wait-threshold(=SECONDS)
This option specifies the query run time threshold which is used by
xtrabackup to detect long-running queries with a non-zero value of
`--ftwrl-wait-timeout`. `FLUSH TABLES WITH READ LOCK`
is not started until such long-running queries exist. This option has no
effect if `--ftwrl-wait-timeout` is `0`. Default value
is `60` seconds. Where supported xtrabackup will
automatically use [Backup Locks](https://www.percona.com/doc/percona-server/8.0/management/backup_locks.html#backup-locks)
as a lightweight alternative to `FLUSH TABLES WITH READ LOCK` to copy
non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.


### --ftwrl-wait-query-type(=all|update)
This option specifies which types of queries are allowed to complete before
xtrabackup will issue the global lock. Default is `all`.


### --galera-info()
This option creates the `xtrabackup_galera_info` file which contains
the local node state at the time of the backup. Option should be used when
performing the backup of *Percona XtraDB Cluster*. It has no effect when
backup locks are used to create the backup.


### --generate-new-master-key()
Generate a new master key when doing a copy-back.


### --generate-transition-key()
*xtrabackup* needs to access the same keyring file or vault server
during prepare and copy-back but it should not depend on whether the
server keys have been purged.

`--generate-transition-key` creates and adds to the keyring
a transition key for *xtrabackup* to use if the master key used for
encryption is not found because it has been rotated and purged.


### --get-server-public-key()
Get the server public key


### --help()
When run with this option or without any options *xtrabackup* displays
information about how to run the program on the command line along with all
supported options and variables with default values where appropriate.


### --history(=NAME)
This option enables the tracking of backup history in the
`PERCONA_SCHEMA.xtrabackup_history` table. An optional history series name
may be specified that will be placed with the history record for the current
backup being taken.


### --host(=HOST)
This option accepts a string argument that specifies the host to use when
connecting to the database server with TCP/IP. It is passed to the mysql
child process without alteration. See **mysql --help** for details.


### --incremental()
This option tells *xtrabackup* to create an incremental backup. It is passed
to the *xtrabackup* child process. When this option is specified, either
`--incremental-lsn` or `--incremental-basedir` can also be
given. If neither option is given, option `--incremental-basedir` is
passed to **xtrabackup** by default, set to the first timestamped
backup directory in the backup base directory.


### --incremental-basedir(=DIRECTORY)
When creating an incremental backup, this is the directory containing the
full backup that is the base dataset for the incremental backups.


### --incremental-dir(=DIRECTORY)
When preparing an incremental backup, this is the directory where the
incremental backup is combined with the full backup to make a new full
backup.


### --incremental-force-scan()
When creating an incremental backup, force a full scan of the data pages in
the instance being backuped even if the complete changed page bitmap data is
available.


### --incremental-history-name(=name)
This option specifies the name of the backup series stored in the
`PERCONA_SCHEMA.xtrabackup_history` history record to base an incremental
backup on. *xtrabackup* will search the history table looking for the most
recent (highest `innodb_to_lsn`), successful backup in the series and take
the to_lsn value to use as the starting `lsn` for the incremental
backup. This will be mutually exclusive with
`--incremental-history-uuid`, `--incremental-basedir` and
`--incremental-lsn`. If no valid lsn can be found (no series by that
name, no successful backups by that name) *xtrabackup* will return with an
error. It is used with the `--incremental` option.


### --incremental-history-uuid(=name)
This option specifies the *UUID* of the specific history record stored in the
`PERCONA_SCHEMA.xtrabackup_history` to base an incremental backup on.
`--incremental-history-name`, `--incremental-basedir` and
`--incremental-lsn`. If no valid lsn can be found (no success record
with that *UUID*) *xtrabackup* will return with an error. It is used with
the –incremental option.


### --incremental-lsn(=LSN)
When creating an incremental backup, you can specify the log sequence number
(LSN) instead of specifying
`--incremental-basedir`. For databases created in 5.1 and
later, specify the LSN as a single 64-bit integer. **ATTENTION**: If
a wrong LSN value is specified (a user  error which *Percona XtraBackup* is
unable to detect), the backup will be unusable. Be careful!


### --innodb([=name])
This option is ignored for MySQL option compatibility.


### --innodb-miscellaneous()
There is a large group of InnoDB options that are normally read from the
`my.cnf` configuration file, so that *xtrabackup* boots up its
embedded InnoDB in the same configuration as your current server. You
normally do not need to specify these explicitly. These options have the
same behavior in InnoDB and XtraDB:


### --keyring-file-data(=FILENAME)
The path to the keyring file. Combine this option with
`--xtrabackup-plugin-dir`.


### --kill-long-queries-timeout(=SECONDS)
This option specifies the number of seconds *xtrabackup* waits between
starting `FLUSH TABLES WITH READ LOCK` and killing those queries that block
it. Default is 0 seconds, which means *xtrabackup* will not attempt to kill
any queries. In order to use this option xtrabackup user should have the
`PROCESS` and `SUPER` privileges. Where supported, *xtrabackup*
automatically uses [Backup Locks](https://www.percona.com/doc/percona-server/8.0/management/backup_locks.html#backup-locks)
as a lightweight alternative to `FLUSH TABLES WITH READ LOCK` to copy
non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.


### --kill-long-query-type(=all|select)
This option specifies which types of queries should be killed to unblock the
global lock. Default is “select”.


### --lock-ddl()
Issue `LOCK TABLES FOR BACKUP` if it is supported by server (otherwise use
`LOCK INSTANCE FOR BACKUP`) at the beginning of the backup to block all DDL
operations.

**NOTE**: Prior to *Percona XtraBackup* 8.0.22-15.0, using a safe-slave-backup stops the SQL replica thread
after the InnoDB tables and before the non-InnoDB tables are backed up.

As of *Percona XtraBackup* 8.0.22-15.0, using a safe-slave-backup option stops the SQL
replica thread before copying the InnoDB files.


### --lock-ddl-per-table()
Lock DDL for each table before xtrabackup starts to copy
it and until the backup is completed.

**NOTE**: As of *Percona XtraBackup* 8.0.15, the –lock-ddl-per-table option is deprecated. Use the –lock-ddl option instead.


### --lock-ddl-timeout()
If `LOCK TABLES FOR BACKUP` or `LOCK INSTANCE FOR BACKUP` does not return
within given timeout, abort the backup.


### --log()
This option is ignored for *MySQL*


### --log-bin()
The base name for the log sequence.


### --log-bin-index(=name)
File that holds the names for binary log files.


### --log-copy-interval(=#)
This option specifies the time interval between checks done by the log
copying thread in milliseconds (default is 1 second).


### --login-path()
Read the given path from the login file.


### --move-back()
Move all the files in a previously made backup from the backup directory to
their original locations. As this option removes backup files, it must be
used with caution.


### --no-backup-locks()
Explicity disables the `--backup-locks` option which is enabled by
default.


### --no-defaults()
The default options are only read from the login file.


### --no-lock()
Use this option to disable table lock with `FLUSH TABLES WITH READ
LOCK`. Use it only if ALL your tables are InnoDB and you **DO NOT CARE**
about the binary log position of the backup. This option shouldn’t be used if
there are any `DDL` statements being executed or if any updates are
happening on non-InnoDB tables (this includes the system MyISAM tables in the
*mysql* database), otherwise it could lead to an inconsistent backup. Where
supported *xtrabackup* will automatically use [Backup Locks](https://www.percona.com/doc/percona-server/8.0/management/backup_locks.html#backup-locks)
as a lightweight alternative to `FLUSH TABLES WITH READ LOCK` to copy
non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.  If
you are considering to use this because your backups are failing to acquire
the lock, this could be because of incoming replication events are preventing
the lock from succeeding. Please try using `--safe-slave-backup` to
momentarily stop the replication replica thread, this may help the backup to
succeed and you do not need to use this option.


### --no-server-version-check()
Implemented in *Percona XtraBackup* 8.0.21.

The `--no-server-version-check` option disables the server version check.

The default behavior runs a check that compares the source system version to the *Percona XtraBackup* version. If the source system version is higher than the XtraBackup version, the backup is aborted with a message.

Adding the option overrides this check, and the backup proceeds, but there may be issues with the backup.

See Server Version and Backup Version Comparison for more information.


### --no-version-check()
This option disables the version check. If you do not pass this option, the
automatic version check is enabled implicitly when *xtrabackup* runs
in the `--backup` mode. To disable the version check, you should pass
explicitly the `--no-version-check` option when invoking *xtrabackup*.

When the automatic version check is enabled, *xtrabackup* performs a
version check against the server on the backup stage after creating a server
connection. *xtrabackup* sends the following information to the server:


* MySQL flavour and version


* Operating system name


* Percona Toolkit version


* Perl version

Each piece of information has a unique identifier. This is a MD5 hash value
that Percona Toolkit uses to obtain statistics about how it is used. This is
a random UUID; no client information is either collected or stored.


### --open-files-limit(=#)
The maximum number of file descriptors to reserve with setrlimit().


### --parallel(=#)
This option specifies the number of threads to use to copy multiple data
files concurrently when creating a backup. The default value is 1 (i.e., no
concurrent transfer). In *Percona XtraBackup* 2.3.10 and newer, this option
can be used with the `--copy-back` option to copy the user
data files in parallel (redo logs and system tablespaces are copied in the
main thread).


### --password(=PASSWORD)
This option specifies the password to use when connecting to the database.
It accepts a string argument. See **mysql --help** for details.


### --plugin-load()
List of plugins to load.


### --port(=PORT)
This option accepts a string argument that specifies the port to use when
connecting to the database server with TCP/IP. It is passed to the
**mysql** child process without alteration. See **mysql
--help** for details.


### --prepare()
Makes **xtrabackup** perform a recovery on a backup created with
`--backup`, so that it is ready to use. See
preparing a backup.


### --print-defaults()
Print the program argument list and exit. Must be given as the first option
on the command-line.


### --print-param()
Makes **xtrabackup** print out parameters that can be used for
copying the data files back to their original locations to restore them.


### --read-buffer-size()
Set the datafile read buffer size, given value is scaled up to page size. Default
is 10Mb.


### --rebuild-indexes()
Rebuilds indexes in a compact backup. This option only has effect when the
`--prepare` and `--rebuild-threads` options are provided.


### --rebuild-threads(=#)
Uses the given number of threads to rebuild indexes in a compact backup. This
option only has effect with the `--prepare` and
`--rebuild-indexes` options.


### --remove-original()
Implemented in *Percona XtraBackup* 2.4.6, this option when specified will
remove `.qp`, `.xbcrypt` and `.qp.xbcrypt` files after
decryption and decompression.


### --rocksdb-datadir()
RocksDB data directory


### --rocksdb-wal-dir()
RocksDB WAL directory.


### --rocksdb-checkpoint-max-age()
The checkpoint cannot be older than this number of seconds when the backup
completes.


### --rocksdb-checkpoint-max-count()
Complete the backup even if the checkpoint age requirement has not been met after
this number of checkpoints.


### --rollback-prepared-trx()
Force rollback prepared InnoDB transactions.


### --rsync()
Uses the **rsync** utility to optimize local file transfers. When this
option is specified, *xtrabackup* uses **rsync** to copy
all non-InnoDB files instead of spawning a separate **cp** for each
file, which can be much faster for servers with a large number of databases
or tables.  This option cannot be used together with `--stream`.


### --safe-slave-backup()
When specified, xtrabackup will stop the replica SQL thread just before
running `FLUSH TABLES WITH READ LOCK` and wait to start backup until
`Slave_open_temp_tables` in `SHOW STATUS` is zero. If there are no open
temporary tables, the backup will take place, otherwise the SQL thread will
be started and stopped until there are no open temporary tables. The backup
will fail if `Slave_open_temp_tables` does not become zero after
`--safe-slave-backup-timeout` seconds. The replication SQL
thread will be restarted when the backup finishes. This option is
implemented in order to deal with [replicating temporary tables](https://dev.mysql.com/doc/refman/5.7/en/replication-features-temptables.html)
and isn’t neccessary with Row-Based-Replication.


### --safe-slave-backup-timeout(=SECONDS)
How many seconds `--safe-slave-backup` should wait for
`Slave_open_temp_tables` to become zero. Defaults to 300 seconds.


### --secure-auth()
Refuse client connecting to server if it uses old (pre-4.1.1) protocol.
(Enabled by default; use –skip-secure-auth to disable.)


### --server-id(=#)
The server instance being backed up.


### --server-public-key-path()
The file path to the server public RSA key in the PEM format.


### --skip-tables-compatibility-check()
See `--tables-compatibility-check`.


### --slave-info()
This option is useful when backing up a replication replica server. It prints
the binary log position of the source server. It also writes the binary log
coordinates to the `xtrabackup_slave_info` file as a `CHANGE MASTER`
command. A new replica for this source can be set up by starting a replica server
on this backup and issuing a `CHANGE MASTER` command with the binary log
position saved in the `xtrabackup_slave_info` file.


### --socket()
This option accepts a string argument that specifies the socket to use when
connecting to the local database server with a UNIX domain socket. It is
passed to the mysql child process without alteration. See **mysql
--help** for details.


### --ssl()
Enable secure connection. More information can be found in [–ssl](https://dev.mysql.com/doc/refman/8.0/en/encrypted-connection-options.html)
MySQL server documentation.


### --ssl-ca()
Path of the file which contains list of trusted SSL CAs. More information
can be found in [–ssl-ca](https://dev.mysql.com/doc/refman/8.0/en/encrypted-connection-options.html#option_general_ssl-ca)
MySQL server documentation.


### --ssl-capath()
Directory path that contains trusted SSL CA certificates in PEM format. More
information can be found in [–ssl-capath](https://dev.mysql.com/doc/refman/8.0/en/encrypted-connection-options.html#option_general_ssl-capath)
MySQL server documentation.


### --ssl-cert()
Path of the file which contains X509 certificate in PEM format. More
information can be found in [–ssl-cert](https://dev.mysql.com/doc/refman/8.0/en/encrypted-connection-options.html#option_general_ssl-cert)
MySQL server documentation.


### --ssl-cipher()
List of permitted ciphers to use for connection encryption. More information
can be found in [–ssl-cipher](https://dev.mysql.com/doc/refman/8.0/en/encrypted-connection-options.html#option_general_ssl-cipher)
MySQL server documentation.


### --ssl-crl()
Path of the file that contains certificate revocation lists. More
information can be found in [–ssl-crl](https://dev.mysql.com/doc/refman/8.0/en/encrypted-connection-options.html#option_general_ssl-crl)
MySQL server documentation.


### --ssl-crlpath()
Path of directory that contains certificate revocation list files. More
information can be found in [–ssl-crlpath](https://dev.mysql.com/doc/refman/8.0/en/encrypted-connection-options.html#option_general_ssl-crlpath)
MySQL server documentation.


### --ssl-fips-mode()
SSL FIPS mode (applies only for OpenSSL); permitted values are: *OFF*, *ON*,
*STRICT*.


### --ssl-key()
Path of file that contains X509 key in PEM format. More information can be
found in [–ssl-key](https://dev.mysql.com/doc/refman/8.0/en/encrypted-connection-options.html#option_general_ssl-key)
MySQL server documentation.


### --ssl-mode()
Security state of connection to server. More information can be found in
[–ssl-mode](https://dev.mysql.com/doc/refman/8.0/en/encrypted-connection-options.html#option_general_ssl-mode)
MySQL server documentation.


### --ssl-verify-server-cert()
Verify server certificate Common Name value against host name used when
connecting to server. More information can be found in
[–ssl-verify-server-cert](https://dev.mysql.com/doc/refman/8.0/en/encrypted-connection-options.html#option_general_ssl-verify-server-cert)
MySQL server documentation.


### --stats()
Causes **xtrabackup** to scan the specified data files and print out
index statistics.


### --stream(=FORMAT)
Stream all backup files to the standard output in the specified format.
Currently, this option only supports the xbstream format.


### --strict()
If this option is specified, *xtrabackup* fails with an error when invalid
parameters are passed.


### --tables(=name)
A regular expression against which the full tablename, in
`databasename.tablename` format, is matched. If the name matches, the
table is backed up. See partial backups.


### --tables-compatibility-check()
Enables the engine compatibility warning. The default value is
ON. To disable the engine compatibility warning use
`--skip-tables-compatibility-check`.


### --tables-exclude(=name)
Filtering by regexp for table names. Operates the same
way as `--tables`, but matched names are excluded from
backup. Note that this option has a higher priority than
`--tables`.


### --tables-file(=name)
A file containing one table name per line, in databasename.tablename format.
The backup will be limited to the specified tables.


### --target-dir(=DIRECTORY)
This option specifies the destination directory for the backup. If the
directory does not exist, **xtrabackup** creates it. If the directory
does exist and is empty, **xtrabackup** will succeed.
**xtrabackup** will not overwrite existing files, however; it will
fail with operating system error 17, `file exists`.

If this option is a relative path, it is interpreted as being relative to
the current working directory from which **xtrabackup** is executed.

In order to perform a backup, you need `READ`, `WRITE`, and `EXECUTE`
permissions at a filesystem level for the directory that you supply as the
value of `--target-dir`.


### --innodb-temp-tablespaces-dir(=DIRECTORY)
Directory where temp tablespace files live, this path can be absolute.


### --throttle(=#)
This option limits the number of chunks copied per second. The chunk size is
*10 MB*. To limit the bandwidth to *10 MB/s*, set the option to *1*:
–throttle=1.


### --tls-ciphersuites()
TLS v1.3 cipher to use.


### --tls-version()
TLS version to use, permitted values are: *TLSv1*, *TLSv1.1*,
*TLSv1.2*, *TLSv1.3*.


### --tmpdir(=name)
Specify the directory that will be used to store temporary files during the
backup


### --transition-key(=name)
This option is used to enable processing the backup without accessing the
keyring vault server. In this case, **xtrabackup** derives the AES
encryption key from the specified passphrase and uses it to encrypt
tablespace keys of tablespaces being backed up.

If `--transition-key` does not have any
value, **xtrabackup** will ask for it. The same passphrase should be
specified for the `--prepare` command.


### --use-memory()
This option affects how much memory is allocated for preparing a backup with
`--prepare`, or analyzing statistics with
`--stats`. Its purpose is similar
to innodb_buffer_pool_size. It does not do the same thing as the
similarly named option in Oracle’s InnoDB Hot Backup tool.
The default value is 100MB, and if you have enough available memory, 1GB to
2GB is a good recommended value. Multiples are supported providing the unit
(e.g. 1MB, 1M, 1GB, 1G).


### --user(=USERNAME)
This option specifies the MySQL username used when connecting to the server,
if that’s not the current user. The option accepts a string argument. See
mysql –help for details.


### -v()
See `--version`


### --version()
This option prints *xtrabackup* version and exits.


### --xtrabackup-plugin-dir(=DIRNAME)
The absolute path to the directory that contains the `keyring` plugin.

<!-- *xtrabackup* replace:: :program:`xtrabackup` -->
