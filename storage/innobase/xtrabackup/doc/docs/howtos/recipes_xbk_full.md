# Making a Full Backup

Backup the InnoDB data and log files - located in `/var/lib/mysql/` - to
`/data/backups/mysql/` (destination). Then, prepare the backup files to be
ready to restore or use (make the data files consistent).

## Make a backup:

```bash
$ xtrabackup --backup --target-dir=/data/backups/mysql/
```

## Prepare the backup twice:

```bash
$ xtrabackup --prepare --target-dir=/data/backups/mysql/
$ xtrabackup --prepare --target-dir=/data/backups/mysql/
```

## Success Criterion

* The exit status of xtrabackup is 0.

* In the second xtrabackup –prepare step, you should see InnoDB print messages
similar to `Log file ./ib_logfile0 did not exist: new to be created`,
followed by a line indicating the log file was created (creating new logs is
the purpose of the second preparation).

## Notes

* You might want to set the `xtrabackup --use-memory` option to
something similar to the size of your buffer pool, if you are on a dedicated
server that has enough free memory. More details [here](../xtrabackup_bin/xbk_option_reference.md).

* For a more detailed explanation, see [Creating a backup](../backup_scenarios/full_backup.md#creating-a-backup)
