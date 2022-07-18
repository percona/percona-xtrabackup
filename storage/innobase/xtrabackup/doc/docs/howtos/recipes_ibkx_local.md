# Make a Local Full Backup (Create, Prepare and Restore)

## Create the Backup

This is the simplest use case. It copies all your *MySQL* data into the specified directory. Here is how to make a backup of all the databases in the `datadir` specified in your `my.cnf`. It will put the backup in a time stamped subdirectory of /data/backups/, in this case, `/data/backups/2010-03-13_02-42-44`,

```default
$ innobackupex /data/backups
```

There is a lot of output, but you need to make sure you see this at the end of the backup. If you don’t see this output, then your backup failed:

```default
100313 02:43:07  innobackupex: completed OK!
```

## Prepare the Backup

To prepare the backup use the `innobackupex --apply-log` option and specify the timestamped subdirectory of the backup. To speed up the apply-log process, use the `innobackupex --use-memory`:

```bash
$ innobackupex --use-memory=4G --apply-log /data/backups/2010-03-13_02-42-44/
```

You should check for a confirmation message:

```default
100313 02:51:02  innobackupex: completed OK!
```

Now the files in `/data/backups/2010-03-13_02-42-44` is ready to be used by the server.

## Restore the Backup

To restore the already-prepared backup, first stop the server and then use the `innobackupex --copy-back` function of innobackupex:

```default
innobackupex --copy-back /data/backups/2010-03-13_02-42-44/
## Use chmod to correct the permissions, if necessary!
```

This will copy the prepared data back to its original location as defined by the `datadir` in your `my.cnf`.

!!! note

    The `datadir` must be empty; *Percona XtraBackup* `innobackupex --copy-back` option will not copy over existing files unless `innobackupex --force-non-empty-directories` option is specified. Also it’s important to note that *MySQL* server needs to be shut down before restore is performed. You can’t restore to a `datadir` of a running mysqld instance (except when importing a partial backup).

After the confirmation message:

```default
100313 02:58:44  innobackupex: completed OK!
```

you should check the file permissions after copying the data back. You may need to adjust them with something like:

```default
$ chown -R mysql:mysql /var/lib/mysql
```

Now the `datadir` contains the restored data. You are ready to start the server.
