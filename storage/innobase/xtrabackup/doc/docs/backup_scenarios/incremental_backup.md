# Incremental Backup

*xtrabackup* supports incremental backups, which means that they can copy
only
all the data that has changed since the last backup.

**NOTE**: Incremental backups on the MyRocks storage engine do not
determine if an earlier full backup or incremental backup contains the same
files. **Percona XtraBackup** copies all the MyRocks files each time it
takes a backup.

You can perform many incremental backups between each full backup, so you
can
set up a backup process such as a full backup once a week and an
incremental
backup every day, or full backups every day and incremental backups every
hour.

Incremental backups work because each *InnoDB* page contains a log sequence
number, or LSN. The LSN is the system version number for the
entire database. Each page’s LSN shows how recently it was changed.

An incremental backup copies each page whose LSN is newer than the
previous incremental or full backup’s LSN. There are two algorithms in
use to find the set of such pages to be copied. The first one, available
with
all the server types and versions, is to check the page LSN directly by
reading all the data pages. The second one, available with *Percona Server
for MySQL*, is
to enable
the [changed page tracking](http://www.percona.com/doc/percona-server/5.6/management/changed_page_tracking.html)
feature on the server, which will note the pages as they are being changed.
This information will be then written out in a compact separate so-called
bitmap file. The *xtrabackup* binary will use that file to read only the
data
pages it needs for the incremental backup, potentially saving many read
requests. The latter algorithm is enabled by default if the *xtrabackup*
binary
finds the bitmap file. It is possible to specify
`--incremental-force-scan` to read all the pages even if the
bitmap data is available.

## Creating an Incremental Backup

To make an incremental backup, begin with a full backup as usual. The
*xtrabackup* binary writes a file called `xtrabackup_checkpoints` into
the backup’s target directory. This file contains a line showing the
`to_lsn`, which is the database’s LSN at the end of the backup.
Create the full backup with a following command:

```
$ xtrabackup --backup --target-dir=/data/backups/base
```

If you look at the `xtrabackup_checkpoints` file, you should see similar
content depending on your LSN nuber:

```
backup_type = full-backuped
from_lsn = 0
to_lsn = 1626007
last_lsn = 1626007
compact = 0
recover_binlog_info = 1
```

Now that you have a full backup, you can make an incremental backup based
on
it. Use the following command:

```
$ xtrabackup --backup --target-dir=/data/backups/inc1 \
--incremental-basedir=/data/backups/base
```

The `/data/backups/inc1/` directory should now contain delta files, such
as `ibdata1.delta` and `test/table1.ibd.delta`. These represent the
changes since the `LSN 1626007`. If you examine the
`xtrabackup_checkpoints` file in this directory, you should see similar
content to the following:

```
backup_type = incremental
from_lsn = 1626007
to_lsn = 4124244
last_lsn = 4124244
compact = 0
recover_binlog_info = 1
```

`from_lsn` is the starting LSN of the backup and for incremental it has to
be
the same as `to_lsn` (if it is the last checkpoint) of the previous/base
backup.

It’s now possible to use this directory as the base for yet another
incremental
backup:

```
$ xtrabackup --backup --target-dir=/data/backups/inc2 \
--incremental-basedir=/data/backups/inc1
```

This folder also contains the `xtrabackup_checkpoints`:

```
backup_type = incremental
from_lsn = 4124244
to_lsn = 6938371
last_lsn = 7110572
compact = 0
recover_binlog_info = 1
```

**NOTE**: In this case you can see that there is a difference between
the `to_lsn`
(last checkpoint LSN) and `last_lsn` (last copied LSN), this means that
there was some traffic on the server during the backup process.

## Preparing the Incremental Backups

The `--prepare` step for incremental backups is not the same
as for full backups. In full backups, two types of operations are performed
to
make the database consistent: committed transactions are replayed from the
log
file against the data files, and uncommitted transactions are rolled back.
You
must skip the rollback of uncommitted transactions when preparing an
incremental backup, because transactions that were uncommitted at the time
of
your backup may be in progress, and it’s likely that they will be committed
in
the next incremental backup. You should use the
`--apply-log-only` option to prevent the rollback phase.

**WARNING**: **If you do not use the** `--apply-log-only` **option to
prevent the rollback phase, then your incremental backups will be useless**
.
After transactions have been rolled back, further incremental backups
cannot
be applied.

Beginning with the full backup you created, you can prepare it, and then
apply
the incremental differences to it. Recall that you have the following
backups:

```
/data/backups/base
/data/backups/inc1
/data/backups/inc2
```

To prepare the base backup, you need to run `--prepare` as
usual, but prevent the rollback phase:

```
$ xtrabackup --prepare --apply-log-only --target-dir=/data/backups/base
```

The output should end with text similar to the following:

```
InnoDB: Shutdown completed; log sequence number 1626007
161011 12:41:04 completed OK!
```

The log sequence number should match the `to_lsn` of the base backup, which
you saw previously.

**NOTE**: This backup is actually safe to restore as-is
now, even though the rollback phase has been skipped. If you restore it and
start *MySQL*, *InnoDB* will detect that the rollback phase was not
performed, and it will do that in the background, as it usually does for a
crash recovery upon start. It will notify you that the database was not
shut
down normally.

To apply the first incremental backup to the full backup, run the following
command:

```
$ xtrabackup --prepare --apply-log-only --target-dir=/data/backups/base \
--incremental-dir=/data/backups/inc1
```

This applies the delta files to the files in `/data/backups/base`, which
rolls them forward in time to the time of the incremental backup. It then
applies the redo log as usual to the result. The final data is in
`/data/backups/base`, not in the incremental directory. You should see
an output similar to:

```
incremental backup from 1626007 is enabled.
xtrabackup: cd to /data/backups/base
xtrabackup: This target seems to be already prepared with --apply-log-only.
xtrabackup: xtrabackup_logfile detected: size=2097152, start_lsn=(4124244)
...
xtrabackup: page size for /tmp/backups/inc1/ibdata1.delta is 16384 bytes
Applying /tmp/backups/inc1/ibdata1.delta to ./ibdata1...
...
161011 12:45:56 completed OK!
```

Again, the LSN should match what you saw from your earlier inspection of
the
first incremental backup. If you restore the files from
`/data/backups/base`, you should see the state of the database as of the
first incremental backup.

**WARNING**: *Percona XtraBackup* does not support using the same
incremental backup directory to prepare
two copies of backup. Do not run `--prepare` with the same
incremental backup directory (the value of –incremental-dir) more than
once.

Preparing the second incremental backup is a similar process: apply the
deltas
to the (modified) base backup, and you will roll its data forward in time
to
the point of the second incremental backup:

```
$ xtrabackup --prepare --target-dir=/data/backups/base \
--incremental-dir=/data/backups/inc2
```

**NOTE**: `--apply-log-only` should be used when merging the 
incremental backups
except
the last one. That’s why the previous line does not contain the
`--apply-log-only` option. Even if the `--apply-log-only` was
used on the last step, backup would still be consistent but in that case
server
would perform the rollback phase.

Once prepared incremental backups are the same as the full backups, and 
they
can be restored in the same
way.
