# Creating a Backup with innobackupex

innobackupex is the tool which provides functionality to backup a whole MySQL
database instance using the xtrabackup in combination with tools like
xbstream and xbcrypt.

To create a full backup, invoke the script with the options needed to connect to
the server and only one argument: the path to the directory where the backup
will be stored

```default
$ innobackupex --user=DBUSER --password=DBUSERPASS /path/to/BACKUP-DIR/
```

and check the last line of the output for a confirmation message:

```console
innobackupex: Backup created in directory '/path/to/BACKUP-DIR/2013-03-25_00-00-09'
innobackupex: MySQL binlog position: filename 'mysql-bin.000003', position 1946
111225 00:00:53  innobackupex: completed OK!
```

The backup will be stored within a time stamped directory created in the
provided path, `/path/to/BACKUP-DIR/2013-03-25_00-00-09` in this
particular example.

## Under the hood

innobackupex called xtrabackup binary to backup all the data of *InnoDB*
tables (see [Creating a backup](../backup_scenarios/full_backup.md#creating-a-backup) for details on this
process) and copied all the table definitions in the database (.frm
files), data and files related to *MyISAM*, MERGE <.MRG> (reference to
other tables), `CSV <.CSV>` and `ARCHIVE <.ARM>` tables, along with
`triggers <.TRG>` and `database configuration information <.opt>` to
a time stamped directory created in the provided path.

It will also create the [following files](../xtrabackup-files.md#xtrabackup-files) for
convenience on the created directory.

## `innobackupex --no-timestamp`

This option tells innobackupex not to create a time stamped directory to store the backup:

```default
$ innobackupex --user=DBUSER --password=DBUSERPASS /path/to/BACKUP-DIR/ --no-timestamp
```

innobackupex will create the `BACKUP-DIR` subdirectory (or fail if exists) and store the backup inside of it.

## `innobackupex --defaults-file`

You can provide another configuration file to innobackupex with this option. The
only limitation is that **it has to be the first option passed**:

```default
$ innobackupex --defaults-file=/tmp/other-my.cnf --user=DBUSER --password=DBUSERPASS /path/to/BACKUP-DIR/
```
