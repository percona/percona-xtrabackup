# Restoring Individual Tables

In server versions prior to 5.6, it is not possible to copy tables between
servers by copying the files, even with `innodb_file_per_table`. However,
with the *Percona XtraBackup*, you can export individual tables from any
*InnoDB* database, and import them into *Percona Server* with *XtraDB* or
*MySQL* 5.6 (The source doesn’t have to be *XtraDB* or or *MySQL* 5.6, but the
destination does). This only works on individual `.ibd` files, and cannot
export a table that is not contained in its own `.ibd` file.

!!! note

    If you’re running *Percona Server* version older than 5.5.10-20.1, variable [innodb_expand_import](http://www.percona.com/doc/percona-server/5.5/management/innodb_expand_import.html#innodb_expand_import) should be used instead of [innodb_import_table_from_xtrabackup](http://www.percona.com/doc/percona-server/5.5/management/innodb_expand_import.html#innodb_import_table_from_xtrabackup).

## Exporting tables

Exporting is done in the preparation stage, not at the moment of creating the
backup. Once a full backup is created, prepare it with the
`innobackupex --export` option:

```bash
$ innobackupex --apply-log --export /path/to/backup
```

This will create for each *InnoDB* with its own tablespace a file with
`.exp` extension. An output of this procedure would contain:

```default
..
xtrabackup: export option is specified.
xtrabackup: export metadata of table 'mydatabase/mytable' to file
`./mydatabase/mytable.exp` (1 indexes)
..
```

Now you should see a `.exp` file in the target directory:

```default
$ find /data/backups/mysql/ -name export_test.*
/data/backups/mysql/test/export_test.exp
/data/backups/mysql/test/export_test.ibd
/data/backups/mysql/test/export_test.cfg
```

These three files are all you need to import the table into a server running
*Percona Server for MySQL* with XtraDB or *MySQL* 5.6.

!!! note

    *MySQL* uses `.cfg` file which contains *InnoDB* dictionary dump in special format. This format is different from the `.exp` one which is used in XtraDB for the same purpose. Strictly speaking, a `.cfg` file is **not** required to import a tablespace to *MySQL* 5.6 or *Percona Server for MySQL* 5.6. A tablespace will be imported successfully even if it is from another server, but *InnoDB* will do schema validation if the corresponding `.cfg` file is present in the same directory.

Each `.exp` (or `.cfg`)  file will be used for importing that table.

!!! note

    InnoDB does a slow shutdown (i.e. full purge + change buffer merge) on --export, otherwise the tablespaces wouldn’t be consistent and thus couldn’t be imported. All the usual performance considerations apply: sufficient buffer pool (i.e. `--use-memory`, 100MB by default) and fast enough storage, otherwise it can take a prohibitive amount of time for export to complete.

## Importing tables

To import a table to other server, first create a new table with the same
structure as the one that will be imported at that server:

```default
OTHERSERVER|mysql> CREATE TABLE mytable (...) ENGINE=InnoDB;
```

then discard its tablespace:

```default
OTHERSERVER|mysql> ALTER TABLE mydatabase.mytable DISCARD TABLESPACE;
```

Next, copy `mytable.ibd` and `mytable.exp` ( or `mytable.cfg`
if importing to *MySQL* 5.6) files to database’s home, and import its
tablespace:

```default
OTHERSERVER|mysql> ALTER TABLE mydatabase.mytable IMPORT TABLESPACE;
```

Set the owner and group of the files:

```bash
$ chown -R mysql:mysql /datadir/db_name/table_name.*
```

After running this command, data in the imported table will be available.
