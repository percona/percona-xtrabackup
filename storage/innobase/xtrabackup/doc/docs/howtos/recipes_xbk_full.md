# Making a Full Backup

Backup the InnoDB data and log files located in `/var/lib/mysql/` to
`/data/backups/mysql/` (destination). Then, prepare the backup files to be
ready to restore or use (make the data files consistent).

### Making a backup

```
$ xtrabackup --backup --target-dir=/data/backup/mysql/
```

### Preparing the backup twice

```
$ xtrabackup --prepare --target-dir=/data/backup/mysql/
$ xtrabackup --prepare --target-dir=/data/backup/mysql/
```

### Success Criteria

* The exit status of xtrabackup is 0.


* In the second `--prepare` step, you should see InnoDB print messages
  similar to `Log file ./ib_logfile0 did not exist: new to be created`,
  followed by a line indicating the log file was created (creating new logs
  is
  the purpose of the second preparation).

**NOTE**: You might want to set the `--use-memory` option to a value close
to the size of your buffer pool, if you are on a dedicated server that has
enough free memory. The [xtrabackup Option reference](https://docs.percona.com/percona-xtrabackup/latest/xtrabackup_bin/xbk_option_reference.html#xbk-option-reference) contains more details.

Review [Full Backups](https://docs.percona.com/percona-xtrabackup/latest/backup_scenarios/full_backup.html#creating-a-backup) for a more detailed explanation.
