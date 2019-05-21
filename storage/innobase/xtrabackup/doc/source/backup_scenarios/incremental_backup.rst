.. _incremental_backup:

==================
Incremental Backup
==================

Both |xtrabackup| and |innobackupex| tools supports incremental backups,
which means that they can copy only the data that has changed since the last
backup.

You can perform many incremental backups between each full backup, so you can
set up a backup process such as a full backup once a week and an incremental
backup every day, or full backups every day and incremental backups every hour.

Incremental backups work because each |InnoDB| page contains a log sequence
number, or :term:`LSN`. The :term:`LSN` is the system version number for the
entire database. Each page's :term:`LSN` shows how recently it was changed.

An incremental backup copies each page whose :term:`LSN` is newer than the
previous incremental or full backup's :term:`LSN`. There are two algorithms in
use to find the set of such pages to be copied. The first one, available with
all the server types and versions, is to check the page :term:`LSN` directly by
reading all the data pages. The second one, available with |Percona Server|, is
to enable the `changed page tracking
<http://www.percona.com/doc/percona-server/5.6/management/changed_page_tracking.html>`_
feature on the server, which will note the pages as they are being changed.
This information will be then written out in a compact separate so-called
bitmap file. The |xtrabackup| binary will use that file to read only the data
pages it needs for the incremental backup, potentially saving many read
requests. The latter algorithm is enabled by default if the |xtrabackup| binary
finds the bitmap file. It is possible to specify
:option:`xtrabackup --incremental-force-scan` to read all the pages even if the
bitmap data is available.

Incremental backups do not actually compare the data files to the previous
backup's data files. In fact, you can use
:option:`xtrabackup --incremental-lsn` to perform an incremental backup without
even having the previous backup, if you know its :term:`LSN`. Incremental
backups simply read the pages and compare their :term:`LSN` to the last
backup's :term:`LSN`. You still need a full backup to recover the incremental
changes, however; without a full backup to act as a base, the incremental
backups are useless.

Creating an Incremental Backup
==============================

To make an incremental backup, begin with a full backup as usual. The
|xtrabackup| binary writes a file called :file:`xtrabackup_checkpoints` into
the backup's target directory. This file contains a line showing the
``to_lsn``, which is the database's :term:`LSN` at the end of the backup.
:ref:`Create the full backup <full_backup>` with a following command:

.. code-block:: bash

  $ xtrabackup --backup --target-dir=/data/backups/base

If you look at the :file:`xtrabackup_checkpoints` file, you should see similar
content depending on your LSN nuber:

.. code-block:: text

  backup_type = full-backuped
  from_lsn = 0
  to_lsn = 1626007
  last_lsn = 1626007
  compact = 0
  recover_binlog_info = 1

Now that you have a full backup, you can make an incremental backup based on
it. Use the following command:

.. code-block:: bash

  $ xtrabackup --backup --target-dir=/data/backups/inc1 \
  --incremental-basedir=/data/backups/base

The :file:`/data/backups/inc1/` directory should now contain delta files, such
as :file:`ibdata1.delta` and :file:`test/table1.ibd.delta`. These represent the
changes since the ``LSN 1626007``. If you examine the
:file:`xtrabackup_checkpoints` file in this directory, you should see similar
content to the following:

.. code-block:: text

  backup_type = incremental
  from_lsn = 1626007
  to_lsn = 4124244
  last_lsn = 4124244
  compact = 0
  recover_binlog_info = 1

``from_lsn`` is the starting LSN of the backup and for incremental it has to be
the same as ``to_lsn`` (if it is the last checkpoint) of the previous/base
backup.

It's now possible to use this directory as the base for yet another incremental
backup:

.. code-block:: bash

  $ xtrabackup --backup --target-dir=/data/backups/inc2 \
  --incremental-basedir=/data/backups/inc1

This folder also contains the :file:`xtrabackup_checkpoints`:

.. code-block:: text

  backup_type = incremental
  from_lsn = 4124244
  to_lsn = 6938371
  last_lsn = 7110572
  compact = 0
  recover_binlog_info = 1

.. note::

  In this case you can see that there is a difference between the ``to_lsn``
  (last checkpoint LSN) and ``last_lsn`` (last copied LSN), this means that
  there was some traffic on the server during the backup process.

.. _preparing_incremental_backups:

Preparing the Incremental Backups
=================================

The :option:`xtrabackup --prepare` step for incremental backups is not the same
as for full backups. In full backups, two types of operations are performed to
make the database consistent: committed transactions are replayed from the log
file against the data files, and uncommitted transactions are rolled back. You
must skip the rollback of uncommitted transactions when preparing an
incremental backup, because transactions that were uncommitted at the time of
your backup may be in progress, and it's likely that they will be committed in
the next incremental backup. You should use the
:option:`xtrabackup --apply-log-only` option to prevent the rollback phase.

.. warning::

  **If you do not use the** :option:`xtrabackup --apply-log-only` **option to
  prevent the rollback phase, then your incremental backups will be useless**.
  After transactions have been rolled back, further incremental backups cannot
  be applied.

Beginning with the full backup you created, you can prepare it, and then apply
the incremental differences to it. Recall that you have the following backups:

.. code-block:: bash

  /data/backups/base
  /data/backups/inc1
  /data/backups/inc2

To prepare the base backup, you need to run :option:`xtrabackup --prepare` as
usual, but prevent the rollback phase:

.. code-block:: bash

  $ xtrabackup --prepare --apply-log-only --target-dir=/data/backups/base

The output should end with text similar to the following:

.. code-block:: text

  InnoDB: Shutdown completed; log sequence number 1626007
  161011 12:41:04 completed OK!

The log sequence number should match the ``to_lsn`` of the base backup, which
you saw previously.

.. note::

  This backup is actually safe to :ref:`restore <restoring_a_backup>` as-is
  now, even though the rollback phase has been skipped. If you restore it and
  start |MySQL|, |InnoDB| will detect that the rollback phase was not
  performed, and it will do that in the background, as it usually does for a
  crash recovery upon start. It will notify you that the database was not shut
  down normally.

To apply the first incremental backup to the full backup, run the following
command:

.. code-block:: bash

  $ xtrabackup --prepare --apply-log-only --target-dir=/data/backups/base \
  --incremental-dir=/data/backups/inc1

This applies the delta files to the files in :file:`/data/backups/base`, which
rolls them forward in time to the time of the incremental backup. It then
applies the redo log as usual to the result. The final data is in
:file:`/data/backups/base`, not in the incremental directory. You should see
an output similar to:

.. code-block:: bash

  incremental backup from 1626007 is enabled.
  xtrabackup: cd to /data/backups/base
  xtrabackup: This target seems to be already prepared with --apply-log-only.
  xtrabackup: xtrabackup_logfile detected: size=2097152, start_lsn=(4124244)
  ...
  xtrabackup: page size for /tmp/backups/inc1/ibdata1.delta is 16384 bytes
  Applying /tmp/backups/inc1/ibdata1.delta to ./ibdata1...
  ...
  161011 12:45:56 completed OK!

Again, the |LSN| should match what you saw from your earlier inspection of the
first incremental backup. If you restore the files from
:file:`/data/backups/base`, you should see the state of the database as of the
first incremental backup.

.. warning::

   |PXB| does not support using the same incremental backup directory to
   prepare two copies of backup. Do not run :option:`xtrabackup
   --prepare` with the same incremental backup directory (the value of
   `--incremental-dir`) more than once.

Preparing the second incremental backup is a similar process: apply the deltas
to the (modified) base backup, and you will roll its data forward in time to
the point of the second incremental backup:

.. code-block:: bash

  $ xtrabackup --prepare --target-dir=/data/backups/base \
  --incremental-dir=/data/backups/inc2

.. note::

 :option:`xtrabackup --apply-log-only` should be used when merging all
 incrementals except the last one. That's why the previous line doesn't contain
 the :option:`xtrabackup --apply-log-only` option. Even if the
 :option:`xtrabackup --apply-log-only` was used on the last step, backup would
 still be consistent but in that case server would perform the rollback phase.

Once prepared, incremental backups are the same as the :ref:`full backups
<full_backup>` and they can be :ref:`restored <restoring_a_backup>` in the same
way.

.. |PXB| replace:: PXB
