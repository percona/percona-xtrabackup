# Partial Backups

*xtrabackup* supports taking partial backups when the
innodb_file_per_table option is enabled. There are three ways to create
partial backups:


1. matching the tables names with a regular expression


2. providing a list of table names in a file


3. providing a list of databases

**WARNING**: Do not copy back the prepared backup.

Restoring partial backups should be done by importing the tables,
not by using the –copy-back option. It is not
recommended to run incremental backups after running a partial
backup.

Although there are some scenarios where restoring can be done by
copying back the files, this may lead to database
inconsistencies in many cases and it is not a recommended way to
do it.

For the purposes of this manual page, we will assume that there is a database
named `test` which contains tables named `t1` and `t2`.

**WARNING**: If any of the matched or listed tables is deleted during the backup,
*xtrabackup* will fail.

## Creating Partial Backups

There are multiple ways of specifying which part of the whole data is backed up:


* Use the `--tables` option to list the table names


* Use the `--tables-file` option to list the tables in a file


* Use the `--databases` option to list the databases


* Use the `--databases-file` option to list the databases

## The –tables Option

The first method involves the xtrabackup –tables option. The option’s
value is a regular expression that is matched against the fully-qualified database name and table name using the `databasename.tablename` format.

To back up only tables in the `test` database, use the following
command:

```
$ xtrabackup --backup --datadir=/var/lib/mysql --target-dir=/data/backups/ \
--tables="^test[.].*"
```

To back up only the `test.t1` table, use the following command:

```
$ xtrabackup --backup --datadir=/var/lib/mysql --target-dir=/data/backups/ \
--tables="^test[.]t1"
```

## The –tables-file Option

The `--tables-file` option specifies a file that can contain multiple table
names, one table name per line in the file. Only the tables named in the file
will be backed up. Names are matched exactly, case-sensitive, with no pattern or
regular expression matching. The table names must be fully-qualified in
`databasename.tablename` format.

```
$ echo "mydatabase.mytable" > /tmp/tables.txt
$ xtrabackup --backup --tables-file=/tmp/tables.txt
```

## The –databases and –databases-file options

The \` –databases\` option accepts a space-separated list of the databases
and tables to backup in the `databasename[.tablename]` format. In addition to
this list, make sure to specify the `mysql`, `sys`, and

`performance_schema` databases. These databases are required when restoring
the databases using xtrabackup –copy-back.

**NOTE**: Tables processed during the –prepare step may also be added to the backup
even if they are not explicitly listed by the parameter if they were created
after the backup started.

```
$ xtrabackup --databases='mysql sys performance_schema test ...'
```

## The `--databases-file` Option

The –databases-file option specifies a file that can contain multiple
databases and tables in the `databasename[.tablename]` format, one element name per line in the file. Names are matched exactly, case-sensitive, with no pattern or regular expression matching.

**NOTE**: Tables processed during the –prepare step may also be added to the backup
even if they are not explicitly listed by the parameter if they were created
after the backup started.

## Preparing Partial Backups

The procedure is analogous to restoring individual tables : apply the logs and use the
–export option:

```
$ xtrabackup --prepare --export --target-dir=/path/to/partial/backup
```

When you use the –prepare option on a partial backup, you
will see warnings about tables that don’t exist. This is because these tables
exist in the data dictionary inside InnoDB, but the corresponding .ibd
files don’t exist. They were not copied into the backup directory. These tables
will be removed from the data dictionary, and when you restore the backup and
start InnoDB, they will no longer exist and will not cause any errors or
warnings to be printed to the log file.

> Could not find any file associated with the tablespace ID: 5

> Use –innodb-directories to find the tablespace files. If that fails then use –innodb-force-recovery=1 to ignore this and to permanently lose all changes to the missing tablespace(s).

## Restoring Partial Backups

Restoring should be done by restoring individual tables in the partial backup to the server.

It can also be done by copying back the prepared backup to a “clean”
datadir (in that case, make sure to include the `mysql`
database) to the datadir you are moving the backup to. A system database can be created with the following:

```
$ sudo mysql --initialize --user=mysql
```

Once you start the server, you may see mysql complaining about missing tablespaces:

```
2021-07-19T12:42:11.077200Z 1 [Warning] [MY-012351] [InnoDB] Tablespace 4, name 'test1/t1', file './d2/test1.ibd' is missing!
2021-07-19T12:42:11.077300Z 1 [Warning] [MY-012351] [InnoDB] Tablespace 4, name 'test1/t1', file './d2/test1.ibd' is missing!
```

In order to clean the orphan database from the data dictionary, you must manually create the missing database directory and then `DROP` this database from the server.

Example of creating the missing database:

```
$ mkdir /var/lib/mysql/test1/d2
```

Example of dropping the database from the server:

```
mysql> DROP DATABASE d2;
Query OK, 2 rows affected (0.5 sec)
```
