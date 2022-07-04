# Partial Backups

*Percona XtraBackup* features partial backups, which means that you may backup
only some specific tables or databases. The tables you back up must be in
separate tablespaces, as a result of being created or altered after you enabled
the `innodb_file_per_table` option on the server.

There is only one caveat about partial backups: do not copy back the prepared
backup. Restoring partial backups should be done by importing the tables, not by
using the traditional `innobackupex --copy-back` option. Although there
are some scenarios where restoring can be done by copying back the files, this
may be lead to database inconsistencies in many cases and it is not the
recommended way to do it.

## Creating Partial Backups

There are three ways of specifying which part of the whole data will be backed
up: regular expressions (`innobackupex --include`), enumerating the
tables in a file (`innobackupex --tables-file`) or providing a list of
databases (`innobackupex --databases`).

### Using `innobackupex --include`

The regular expression provided to this will be matched against the fully
qualified table name, including the database name, in the form
`databasename.tablename`.

For example,

```default
$ innobackupex --include='^mydatabase[.]mytable' /path/to/backup
```

The command above will create a timestamped directory with the usual files that
*innobackupex* creates, but only the data files related to the tables matched.

Note that this option is passed to `xtrabackup --tables` and is matched
against each table of each database, the directories of each database will be
created even if they are empty.

### Using `innobackupex --tables-file`

The text file provided (the path) to this option can contain multiple table
names, one per line, in the `databasename.tablename` format.

For example,

```default
$ echo "mydatabase.mytable" > /tmp/tables.txt
$ innobackupex --tables-file=/tmp/tables.txt /path/to/backup
```

The command above will create a timestamped directory with the usual files that
*innobackupex* creates, but only containing the data-files related to the tables
specified in the file.

This option is passed to `xtrabackup --tables-file` and, unlike the
`--tables <xtrabackup –tables>` option, only directories of databases
of the selected tables will be created.

### Using `innobackupex --databases`

This option accepts either a space-separated list of the databases and tables to
backup - in the `databasename[.tablename]` form.

For example,

```default
$ innobackupex --databases="mydatabase.mytable mysql" /path/to/backup
```

The command above will create a timestamped directory with the usual files that
*innobackupex* creates, but only containing the datafiles related to
`mytable` in the `mydatabase` directory and the `mysql` directory with the
entire `mysql` database.

The `--databases-file` option specifies the path to a text file which contains a case-sensitive list of databases and tables to be backed up. The file can contain the names of multiple databases and tables in a `databasename[.tablename]` format with one element for each line. Only the named databases and tables are backed up. The names must match exactly. There is no pattern matching or regular expression matching.

## Preparing Partial Backups

For preparing partial backups, the procedure is analogous to [restoring
individual tables](restoring_individual_tables_ibk.md) : apply the logs and use
the `innobackupex --export` option:

```default
$ innobackupex --apply-log --export /path/to/partial/backup
```

You may see warnings in the output about tables that don’t exist. This is
because *InnoDB* -based engines stores its data dictionary inside the tablespace
files besides the `.frm` files. *innobackupex* will use *xtrabackup* to
remove the missing tables (those who weren’t selected in the partial backup)
from the data dictionary in order to avoid future warnings or errors:

```default
111225  0:54:06  InnoDB: Error: table 'mydatabase/mytablenotincludedinpartialb'
InnoDB: in InnoDB data dictionary has tablespace id 6,
InnoDB: but tablespace with that id or name does not exist. It will be removed from data dictionary.
```

You should also see the notification of the creation of a file needed for
importing (.exp file) for each table included in the partial backup:

```default
xtrabackup: export option is specified.
xtrabackup: export metadata of table 'employees/departments' to file `.//departments.exp` (2 indexes)
xtrabackup:     name=PRIMARY, id.low=80, page=3
xtrabackup:     name=dept_name, id.low=81, page=4
```

Note that you can use the `innobackupex --export` option with
`innobackupex --apply-log` to an already-prepared backup in order to
create the `.exp` files.

Finally, check for the confirmation message in the output:

```default
111225 00:54:18  innobackupex: completed OK!
```

## Restoring Partial Backups

Restoring should be done by [restoring individual tables](restoring_individual_tables_ibk.md) in the partial backup to the server.

It can also be done by copying back the prepared backup to a “clean”
`datadir` (in that case, make sure to include the `mysql`
database). System database can be created with:

```default
$ sudo mysql_install_db --user=mysql
```
