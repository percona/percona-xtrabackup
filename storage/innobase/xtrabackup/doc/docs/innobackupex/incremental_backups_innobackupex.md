# Incremental Backups with *innobackupex*

As not all information changes between each backup, the incremental backup
strategy uses this to reduce the storage needs and the duration of making a
backup.

This can be done because each *InnoDB* page has a log sequence number, *LSN*,
which acts as a version number of the entire database. Every time the database
is modified, this number gets incremented.

An incremental backup copies all pages since a specific *LSN*.

Once the pages have been put together in their respective order, applying the
logs will recreate the process that affected the database, yielding the data at
the moment of the most recently created backup.

## Creating an Incremental Backups with *innobackupex*

First, you need to make a full backup as the BASE for subsequent incremental backups:

```bash
$ innobackupex /data/backups
```

This will create a timestamped directory in `/data/backups`. Assuming that
the backup is done last day of the month, `BASEDIR` would be
`/data/backups/2013-03-31_23-01-18`, for example.

!!! note

    You can use the `innobackupex --no-timestamp` option to override this behavior and the backup will be created in the given directory.

If you check at the `xtrabackup-checkpoints` file in `BASE-DIR`, you
should see something like:

```default
backup_type = full-backuped
from_lsn = 0
to_lsn = 1626007
last_lsn = 1626007
compact = 0
recover_binlog_info = 1
```

To create an incremental backup the next day, use `innobackupex --incremental` and provide the BASEDIR:

```bash
$ innobackupex --incremental /data/backups --incremental-basedir=BASEDIR
```

Another timestamped directory will be created in `/data/backups`, in this
example, `/data/backups/2013-04-01_23-01-18` containing the incremental
backup. We will call this `INCREMENTAL-DIR-1`.

If you check at the `xtrabackup-checkpoints` file in
`INCREMENTAL-DIR-1`, you should see something like:

```default
backup_type = incremental
from_lsn = 1626007
to_lsn = 4124244
last_lsn = 4124244
compact = 0
recover_binlog_info = 1
```

Creating another incremental backup the next day will be analogous, but this
time the previous incremental one will be base:

```default
$ innobackupex --incremental /data/backups --incremental-basedir=INCREMENTAL-DIR-1
```

Yielding (in this example) `/data/backups/2013-04-02_23-01-18`. We will
use `INCREMENTAL-DIR-2` instead for simplicity.

At this point, the xtrabackup-checkpoints file in `INCREMENTAL-DIR-2`
should contain something like:

```default
backup_type = incremental
from_lsn = 4124244
to_lsn = 6938371
last_lsn = 7110572
compact = 0
recover_binlog_info = 1
```

As it was said before, an incremental backup only copy pages with a *LSN*
greater than a specific value. Providing the *LSN* would have produced
directories with the same data inside:

```default
innobackupex --incremental /data/backups --incremental-lsn=4124244
innobackupex --incremental /data/backups --incremental-lsn=6938371
```

This is a very useful way of doing an incremental backup, since not always the
base or the last incremental will be available in the system.

!!! warning

    This procedure only affects *XtraDB* or *InnoDB*-based tables. Other tables with a different storage engine, e.g. *MyISAM*, will be copied entirely each time an incremental backup is performed.

## Preparing an Incremental Backup with *innobackupex*

Preparing incremental backups is a bit different than full backups. This is, perhaps, the stage where more attention is needed:
 
* First, **only the committed transactions must be replayed on each backup**. This will merge the base full backup with the incremental ones.

* Then, the uncommitted transaction must be rolled back in order to have ready-to-use backup.

If you replay the committed transactions **and** rollback the uncommitted ones
on the base backup, you will not be able to add the incremental ones. If you do
this on an incremental one, you won’t be able to add data from that moment and
the remaining increments.

Having this in mind, the procedure is very straight-forward using the
`innobackupex --redo-only` option, starting with the base backup:

```default
innobackupex --apply-log --redo-only BASE-DIR
```

You should see an output similar to:

```default
160103 22:00:12 InnoDB: Shutdown completed; log sequence number 4124244
160103 22:00:12 innobackupex: completed OK!
```

Then, the first incremental backup can be applied to the base backup, by issuing:

```default
innobackupex --apply-log --redo-only BASE-DIR --incremental-dir=INCREMENTAL-DIR-1
```

You should see an output similar to the previous one but with corresponding *LSN*:

```default
160103 22:08:43 InnoDB: Shutdown completed; log sequence number 6938371
160103 22:08:43 innobackupex: completed OK!
```

If no `innobackupex --incremental-dir` is set, *innobackupex* will use the most
recent subdirectory created in the basedir.

At this moment, `BASE-DIR` contains the data up to the moment of the first
incremental backup. Note that the full data will always be in the directory of
the base backup, as we are appending the increments to it.

Repeat the procedure with the second one:

```bash
$ innobackupex --apply-log BASE-DIR --incremental-dir=INCREMENTAL-DIR-2
```

If the *completed OK!* message was shown, the final data will be in the base
backup directory, `BASE-DIR`.

!!! note

    `innobackupex --redo-only` should be used when merging all incrementals except the last one. That’s why the previous line doesn’t contain the `innobackupex --redo-only` option. Even if the `innobackupex --redo-only` was used on the last step, backup would still be consistent but in that case server would perform the rollback phase.

You can use this procedure to add more increments to the base, as long as you do
it in the chronological order that the backups were done. If you merge the
incrementals in the wrong order, the backup will be useless. If you have doubts
about the order that they must be applied, you can check the file
`xtrabackup_checkpoints` at the directory of each one, as shown in the
beginning of this section.

Once you merge the base with all the increments, you can prepare it to roll back
the uncommitted transactions:

```bash
$ innobackupex --apply-log BASE-DIR
```

Now your backup is ready to be used immediately after restoring it. This
preparation step is optional. However, if you restore without doing the prepare,
the database server will begin to rollback uncommitted transactions, the same
work it would do if a crash had occurred. This results in delay as the database
server starts, and you can avoid the delay if you do the prepare.

Note that the `iblog\*` files will not be created by *innobackupex*, if you
want them to be created, use `xtrabackup --prepare` on the
directory. Otherwise, the files will be created by the server once started.

## Restoring Incremental Backups with *innobackupex*

After preparing the incremental backups, the base directory contains the same
data as the full backup. For restoring it, you can use the `xtrabackup --copy-back` parameter:

```bash
$ xtrabackup --copy-back --target-dir=BASE-DIR
```

If the incremental backup was created using the `xtrabackup --compress`
option, then you need to run `xtrabackup --decompress` followed by
`xtrabackup --copy-back`.

```bash
$ xtrabackup --decompress --target-dir=BASE-DIR
$ xtrabackup --copy-back --target-dir=BASE-DIR
```

You may have to change the ownership as detailed on [Restoring a Full Backup with innobackupex](restoring_a_backup_ibk.md).

## Incremental Streaming Backups using xbstream and tar

Incremental streaming backups can be performed with the *xbstream* streaming
option. Currently backups are packed in custom **xbstream** format. With this
feature, you need to take a BASE backup as well.

### Taking a base backup

```bash
$ innobackupex /data/backups
```

### Taking a local backup

```bash
$ innobackupex --incremental --incremental-lsn=LSN-number --stream=xbstream ./ > incremental.xbstream
```

### Unpacking the backup

```bash
$ xbstream -x < incremental.xbstream
```

### Taking a local backup and streaming it to the remote server and unpacking it

```bash
$ innobackupex  --incremental --incremental-lsn=LSN-number --stream=xbstream ./ | \
ssh user@hostname " cat - | xbstream -x -C > /backup-dir/"
```
