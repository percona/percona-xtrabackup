# Partial Backups

*xtrabackup* supports taking partial backups when the
`innodb_file_per_table` option is enabled. There are three ways to create
partial backups:

1. matching the tables names with a regular expression

2. providing a list of table names in a file

3. providing a list of databases

!!! warning

    If any of the matched or listed tables is deleted during the backup, *xtrabackup* will fail. 
    
    Do not copy back the prepared backup. Restoring partial backups should be done by importing the tables, not by using the `--copy-back` option. It is not recommended to run incremental backups after running a partial backup. 
    
    Although there are some scenarios where restoring can be done by copying back the files, this may lead to database inconsistencies in many cases and it is not a recommended way to do it.

For the purposes of this manual page, we will assume that there is a database
named `test` which contains tables named `t1` and `t2`.

## Using xtrabackup –tables

The first method involves the `xtrabackup --tables` option. The option’s
value is a regular expression that is matched against the fully qualified
tablename, including the database name, in the form `databasename.tablename`.

To back up only tables in the `test` database, you can use the
following command:

```bash
$ xtrabackup --backup --datadir=/var/lib/mysql --target-dir=/data/backups/ \
--tables="^test[.].*"
```

To back up only the table `test.t1`, you can use the following command:

```bash
$ xtrabackup --backup --datadir=/var/lib/mysql --target-dir=/data/backups/ \
--tables="^test[.]t1"
```

## Using `xtrabackup --tables-file`

`xtrabackup --tables-file` specifies a file that can contain multiple
table names, one table name per line in the file. Only the tables named in the
file will be backed up. Names are matched exactly, case-sensitive, with no
pattern or regular expression matching. The table names must be fully qualified,
in `databasename.tablename` format.

```bash
$ echo "mydatabase.mytable" > /tmp/tables.txt
$ xtrabackup --backup --tables-file=/tmp/tables.txt
```

## Using `xtrabackup --databases` and `xtrabackup --databases-file`

`xtrabackup --databases` accepts a space-separated list of the databases
and tables to backup in the format `databasename[.tablename]`. In addition to
this list make sure to specify the `mysql`, `sys`, and
`performance_schema` databases. These databases are required when restoring
the databases using `xtrabackup --copy-back`.

!!! note

    Tables processed during the –prepare step may also be added to the backup even if they are not explicitly listed by the parameter if they were created after the backup started.

```bash
$ xtrabackup --databases='mysql sys performance_schema ...'
```

`xtrabackup --databases-file` specifies a file that can contain multiple
databases and tables in the `databasename[.tablename]` form, one element name
per line in the file. Names are matched exactly, case-sensitive, with no pattern or regular expression matching.

!!! note

    Tables processed during the –prepare step may also be added to the backup even if they are not explicitly listed by the parameter if they were created after the backup started.

## Preparing the Backup

When you use the `xtrabackup --prepare` option on a partial backup, you
will see warnings about tables that don’t exist. This is because these tables
exist in the data dictionary inside InnoDB, but the corresponding `.ibd`
files don’t exist. They were not copied into the backup directory. These tables
will be removed from the data dictionary, and when you restore the backup and
start InnoDB, they will no longer exist and will not cause any errors or
warnings to be printed to the log file.

An example of the error message you will see during the prepare phase
follows.

```default
InnoDB: Reading tablespace information from the .ibd files...
101107 22:31:30  InnoDB: Error: table 'test1/t'
InnoDB: in InnoDB data dictionary has tablespace id 6,
InnoDB: but tablespace with that id or name does not exist. It will be removed from data dictionary.
```
