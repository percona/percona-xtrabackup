# Connection and Privileges Needed

*Percona XtraBackup* needs to be able to connect to the database server and
perform operations on the server and the datadir when creating a
backup, when preparing in some scenarios and when restoring it. In order to do
so, there are privileges and permission requirements on its execution that
must be fulfilled.

Privilege refers to the operations that a system user is permitted to do in
the database server. **They are set at the database server and only apply to
users in the database server**.

Permissions are those which permits a user to perform operations on the system,
like reading, writing or executing on a certain directory or start/stop a
system service. **They are set at a system level and only apply to system
users**.

When *xtrabackup* is used, there are two actors involved: the user invoking the
program - *a system user* - and the user performing action in the database
server - *a database user*. Note that these are different users in different
places, even though they may have the same username.

All the invocations of *xtrabackup* in this documentation assume that the system
user has the appropriate permissions, and you are providing the relevant options
for connecting the database server - besides the options for the action to be
performed - and the database user has adequate privileges.

## Connecting to the server

The database user used to connect to the server and its password are specified
by the `--user` and `--password` option:

```
$ xtrabackup --user=DVADER --password=14MY0URF4TH3R --backup \
--target-dir=/data/bkps/
```

If you don’t use the `--user` option, *Percona XtraBackup* will assume
the database user whose name is the system user executing it.

### Other Connection Options

According to your system, you may need to specify one or more of the following
options to connect to the server:

| **Option** | **Description**                                                  |
|------------|------------------------------------------------------------------|
| -port      | Use this port when connecting to the database with TCP/IP        |
| -socket    | Use this socket when connecting to the local database.           |
| -host      | Use this host when connecting to the database server with TCP/IP |

These options are passed to the **mysql** child process without
alteration, see `mysql --help` for details.


**NOTE**: In case of multiple server instances, the correct connection parameters
(port, socket, host) must be specified in order for *xtrabackup* to talk to
the correct server.

## Permissions and Privileges Needed

Once connected to the server, in order to perform a backup you will need
`READ` and `EXECUTE` permissions at a filesystem level in the
server’s datadir.

The database user needs the following privileges on the tables or databases to be backed up:


* `RELOAD` and `LOCK TABLES` (unless the `--no-lock`
option is specified) in order to run `FLUSH TABLES WITH READ LOCK` and
`FLUSH ENGINE LOGS` prior to start copying the files, and requires this
privilege when [Backup Locks](http://www.percona.com/doc/percona-server/8.0/management/backup_locks.html)
are used


* `BACKUP_ADMIN` privilege is needed to query the
performance_schema.log_status table, and run `LOCK INSTANCE FOR BACKUP`,
`LOCK BINLOG FOR BACKUP`, or `LOCK TABLES FOR BACKUP`.


* `REPLICATION CLIENT` in order to obtain the binary log position,


* `CREATE TABLESPACE` in order to import tables (see Restoring Individual Tables),


* `PROCESS` in order to run `SHOW ENGINE INNODB STATUS` (which is
mandatory), and optionally to see all threads which are running on the
server (see FLUSH TABLES WITH READ LOCK option),


* `SUPER` in order to start/stop the replication threads in a replication
environment, use [XtraDB Changed Page Tracking](https://www.percona.com/doc/percona-server/8.0/management/changed_page_tracking.html)
for Incremental Backups and for handling FLUSH TABLES WITH READ LOCK,


* `CREATE` privilege in order to create the
PERCONA_SCHEMA.xtrabackup_history database and
table,


* `ALTER` privilege in order to upgrade the
PERCONA_SCHEMA.xtrabackup_history database and
table,


* `INSERT` privilege in order to add history records to the
PERCONA_SCHEMA.xtrabackup_history table,


* `SELECT` privilege in order to use
`--incremental-history-name` or
`--incremental-history-uuid` in order for the feature
to look up the `innodb_to_lsn` values in the
PERCONA_SCHEMA.xtrabackup_history table.


* `SELECT` privilege on the [keyring_component_status table](https://dev.mysql.com/doc/refman/8.0/en/performance-schema-keyring-component-status-table.html)  to view the attributes and status of the installed keyring component when in use.

The explanation of when these are used can be found in
How Percona XtraBackup Works.

An SQL example of creating a database user with the minimum privileges required
to full backups would be:

``` sql
mysql> CREATE USER 'bkpuser'@'localhost' IDENTIFIED BY 's3cr%T';
mysql> GRANT BACKUP_ADMIN, PROCESS, RELOAD, LOCK TABLES, REPLICATION CLIENT ON *.* TO 'bkpuser'@'localhost';
mysql> GRANT SELECT ON performance_schema.log_status TO 'bkpuser'@'localhost';
mysql> GRANT SELECT ON performance_schema.keyring_component_status TO bkpuser@'localhost'
mysql> FLUSH PRIVILEGES;
```
