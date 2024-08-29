# Restoring Individual Tables

*Percona XtraBackup* can export a table that is contained in its own .ibd file. With *Percona XtraBackup*, you can export individual tables from any *InnoDB* database, and import them into *Percona Server for MySQL* with *XtraDB* or *MySQL* 8.0. The source doesn’t have to be *XtraDB* or *MySQL* 8.0, but the destination does. This method only works on individual .ibd files.

The following example exports and imports the following table:

```
CREATE TABLE export_test (
a int(11) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
```

## Exporting the Table

Created the table in innodb_file_per_table mode, so
after taking a backup as usual with the –backup option, the
.ibd file exists in the target directory:

```
$ find /data/backups/mysql/ -name export_test.*
/data/backups/mysql/test/export_test.ibd
```

when you prepare the backup, add the –export option to the
command. Here is an example:

```
$ xtrabackup --prepare --export --target-dir=/data/backups/mysql/
```

**NOTE**: If you restore an encrypted InnoDB tablespace table, add the
keyring file:

```
$ xtrabackup --prepare --export --target-dir=/tmp/table \
--keyring-file-data=/var/lib/mysql-keyring/keyring
```

Now you should see an .exp file in the target directory:

```
$ find /data/backups/mysql/ -name export_test.*
/data/backups/mysql/test/export_test.exp
/data/backups/mysql/test/export_test.ibd
/data/backups/mysql/test/export_test.cfg
```

These three files are the only files required to import the table into a server running
*Percona Server for MySQL* with *XtraDB* or *MySQL* 8.0. In case the server uses [InnoDB
Tablespace Encryption](http://dev.mysql.com/doc/refman/5.7/en/innodb-tablespace-encryption.html)
adds an additional .cfp file which contains the transfer key and an encrypted tablespace key.

**NOTE**: The .cfg metadata file contains an *InnoDB* dictionary dump in a special format. This format is different from the .exp one which is
used in *XtraDB* for the same purpose. A .cfg\` file is not required to import a tablespace to *MySQL* 8.0 or Percona
Server for MySQL 8.0.

A tablespace is imported successfully even if the table is from
another server, but *InnoDB* performs a schema validation if the corresponding .cfg file is located in the same directory.

## Importing the Table

On the destination server running *Percona Server for MySQL* with *XtraDB* and
[innodb_import_table_from_xtrabackup](http://www.percona.com/doc/percona-server/8.0/management/innodb_expand_import.html#innodb_import_table_from_xtrabackup)
option enabled, or *MySQL* 8.0, create a table with the same
structure, and then perform the following steps:


1. Run the `ALTER TABLE test.export_test DISCARD TABLESPACE;`
command. If you see the following error, enable
innodb_file_per_table and create the table again.


2. Copy the exported files to the `test/` subdirectory of the destination server’s data directory


3. Run `ALTER TABLE test.export_test IMPORT TABLESPACE;`

The table is imported, and you can run a `SELECT` to see the imported data.
