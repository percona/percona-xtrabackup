# Backing Up and Restoring Individual Partitions

Percona XtraBackup lets you back up
individual partitions because partitions are regular tables with specially formatted names. The only
requirement for this feature is having the innodb_file_per_table option
enabled in the server.

There is one caveat about using this kind of backup: you can not copy back
the prepared backup. Restoring partial backups should be done by importing the
tables.

## Creating the backup

There are three ways of specifying which part of the whole data will be backed
up: regular expressions ( –tables), enumerating the
tables in a file (–tables) or providing a list of
databases (–databases).

The regular expression provided to this option will be matched against the fully
qualified database name and table name, in the form of
`database-name.table-name`.

If the partition 0 is not backed up, Percona XtraBackup cannot generate a .cfg file. MySQL 8.0 stores the table metadata in partition 0.

For example, this operation takes a back-up of the partition `p4` from 
the table `name` located in the database `imdb`:

```
xtrabackup --tables=^imdb[.]name#p#p4 --backup
```

If partition 0 is not backed up, the following errors may occur:

```
xtrabackup: export option not specified
xtrabackup: error: cannot find dictionary record of table imdb/name#p#p4
```

Note that this option is passed to `xtrabackup --tables` and is matched
against each table of each database, the directories of each database will be
created even if they are empty.

## Preparing the backup

For preparing partial backups, the procedure is analogous to restoring
individual tables apply the logs and use xtrabackup –export:

```
xtrabackup --apply-log --export /mnt/backup/2012-08-28_10-29-09
```

You may see warnings in the output about tables that do not exist. This is
because *InnoDB*-based engines stores its data dictionary inside the tablespace
files. *xtrabackup* removes the missing tables (those that haven’t been selected in the partial
backup) from the data dictionary in order to avoid future warnings or errors.

## Restoring from the backups

Restoring should be done by importing the tables in the partial backup to the
server.

First step is to create new table in which data will be restored.

```
CREATE TABLE `name_p4` (
`id` int(11) NOT NULL AUTO_INCREMENT,
`name` text NOT NULL,
`imdb_index` varchar(12) DEFAULT NULL,
`imdb_id` int(11) DEFAULT NULL,
`name_pcode_cf` varchar(5) DEFAULT NULL,
`name_pcode_nf` varchar(5) DEFAULT NULL,
`surname_pcode` varchar(5) DEFAULT NULL,
PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=2812744 DEFAULT CHARSET=utf8
```

**NOTE**: Generate a [.cfg metadata file](https://dev.mysql.com/doc/refman/8.0/en/innodb-table-import.html) by running `FLUSH TABLES ... FOR EXPORT`. The command can only be run on a table, not on the individual table partitions.
The file is located in the table schema directory and is used for schema verification when importing the tablespace.

To restore the partition from the backup, the tablespace must be discarded for
that table:

```
ALTER TABLE name_p4 DISCARD TABLESPACE;
```

The next step is to copy the .exp and ibd files from the backup to the MySQL data directory:

```
cp /mnt/backup/2012-08-28_10-29-09/imdb/name#p#p4.exp /var/lib/mysql/imdb/name_p4.exp
cp /mnt/backup/2012-08-28_10-29-09/imdb/name#P#p4.ibd /var/lib/mysql/imdb/name_p4.ibd
```

**NOTE**: Make sure that the copied files can be accessed by the user running MySQL.

The last step is to import the tablespace:

```
ALTER TABLE name_p4 IMPORT TABLESPACE;
```
