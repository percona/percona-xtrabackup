.. _xb_incremental_ps_56:

=================================================
Incremental Backups with Log Archiving for XtraDB
=================================================

|Percona Server| 5.6.11-60.3 has introduced a new feature called `Log Archiving
for XtraDB
<http://www.percona.com/doc/percona-server/5.6/management/log_archiving.html>`_.
This feature makes copies of the old log files before they are overwritten,
thus saving all the redo log for a write workload.

.. note::

  This feature was removed in Percona Server 5.7, and not supported by |Percona
  XtraBackup| 2.4.


When log archiving is enabled, it duplicates all redo log writes in a separate
set of files in addition to normal redo log writing, creating new files as
necessary.

Archived log file name format is ``ib_log_archive_<startlsn>``. The start LSN
marks the log sequence number when the archive was started. An example of the
archived log files should look like this:

.. code-block:: text

 ib_log_archive_00000000010145937920
 ib_log_archive_00000000010196267520

In order to enable this feature, variable ``innodb_log_archive`` should
be set to ``ON``. Once the feature has been enabled, directory specified with
``innodb_log_arch_dir`` (|MySQL| datadir by default) will contain the
archived log files.

Creating the Backup
===================

To make an incremental backup, begin with a full backup as usual. The
|xtrabackup| binary writes a file called :file:`xtrabackup_checkpoints` into
the backup's target directory. This file contains a line showing the
``to_lsn``, which is the database's :term:`LSN` at the end of the backup.
:ref:`Create the full backup <full_backup>` with a command such as the
following:

.. code-block:: bash

  $ xtrabackup --backup --target-dir=/data/backup/

If you look at the :file:`xtrabackup_checkpoints` file, you should see contents
similar to the following:

.. code-block:: text

  backup_type = full-backuped
  from_lsn = 0
  to_lsn = 430824722
  last_lsn = 438369416
  compact = 0
  recover_binlog_info = 1

Using the Log Archiving to prepare the backup
=============================================

In order to prepare the backup we need to specify the directory that contains
the archived logs with the :option:`xtrabackup --innodb-log-arch-dir` option:

.. code-block:: text

 $ xtrabackup --prepare --target-dir=/data/backup/ \
 --innodb-log-arch-dir=/data/archived-logs/

This command will use archived logs, replay them against the backup folder and
bring them up to date with the latest archived log.

Output should look like this:

.. code-block:: text

  xtrabackup version 2.3.5 based on MySQL server 5.6.24 Linux (x86_64) (revision id: 45cda89)
  xtrabackup: cd to /tmp/backup/
  xtrabackup: using the following InnoDB configuration for recovery:
  xtrabackup:   innodb_data_home_dir = ./
  xtrabackup:   innodb_data_file_path = ibdata1:12M:autoextend
  xtrabackup:   innodb_log_group_home_dir = ./
  xtrabackup:   innodb_log_files_in_group = 2
  xtrabackup:   innodb_log_file_size = 50331648
  xtrabackup: Generating a list of tablespaces
  xtrabackup: using the following InnoDB configuration for recovery:
  xtrabackup:   innodb_data_home_dir = ./
  xtrabackup:   innodb_data_file_path = ibdata1:12M:autoextend
  xtrabackup:   innodb_log_group_home_dir = ./
  xtrabackup:   innodb_log_files_in_group = 2
  xtrabackup:   innodb_log_file_size = 50331648
  xtrabackup: Starting InnoDB instance for recovery.
  xtrabackup: Using 104857600 bytes for buffer pool (set by --use-memory parameter)
  InnoDB: Using atomics to ref count buffer pool pages
  InnoDB: The InnoDB memory heap is disabled
  InnoDB: Mutexes and rw_locks use GCC atomic builtins
  InnoDB: Memory barrier is not used
  InnoDB: Compressed tables use zlib 1.2.8
  InnoDB: Using CPU crc32 instructions
  InnoDB: Initializing buffer pool, size = 100.0M
  InnoDB: Completed initialization of buffer pool
  InnoDB: Setting log file ./ib_logfile101 size to 48 MB
  InnoDB: Setting log file ./ib_logfile1 size to 48 MB
  InnoDB: Renaming log file ./ib_logfile101 to ./ib_logfile0
  InnoDB: New log files created, LSN=15717900
  InnoDB:  Starting archive recovery from a backup...
  InnoDB: Opened archived log file /var/lib/mysql/ib_log_archive_00000000000009079808
  InnoDB: Starting an apply batch of log records to the database...
  InnoDB: Progress in percent: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95 96 97 98 99
  InnoDB: Apply batch completed
  InnoDB: Opened archived log file /var/lib/mysql/ib_log_archive_00000000000059409408
  InnoDB: Starting an apply batch of log records to the database...
  InnoDB: Progress in percent: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95 96 97 98 99
  InnoDB: Apply batch completed
  ...
  InnoDB: Starting an apply batch of log records to the database...
  InnoDB: Progress in percent: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95 96 97 98 99
  InnoDB: Apply batch completed
  InnoDB: 1 transaction(s) which must be rolled back or cleaned up
  InnoDB: in total 15931 row operations to undo
  InnoDB: Trx id counter is 36864

  xtrabackup: starting shutdown with innodb_fast_shutdown = 1
  InnoDB: Starting shutdown...
  InnoDB: Shutdown completed; log sequence number 713694189
  161012 13:48:50 completed OK!

After this is completed successfully backup can be restored.

You can check the :file:`xtrabackup_checkpoints` file and see that the
``backup_type`` has changed:

.. code-block:: text

   backup_type = log-applied
   from_lsn = 0
   to_lsn = 430824722
   last_lsn = 438369416
   compact = 0
   recover_binlog_info = 0

.. note::

   Archived logs can be applied to backup data several times, for example to
   decrease the backup size or time required for preparing the backup.

Additional option is available if you need to restore a backup to specific
point in time. By adding the :option:`xtrabackup --to-archived-lsn` option you
can specify the LSN to which the backup will be prepared.

.. code-block:: bash

  $ xtrabackup --prepare --target-dir=/data/backup/ \
  --innodb-log-arch-dir=/data/archived-logs/ --to-archived-lsn=5536301566

This will prepare the backup up to the specified Log Sequence Number.

.. note::

  When restoring this backup, due to bugs :bug:`1632734` and :bug:`1632737`
  you'll need to either restore the backup as usual and let server to cleanup
  on startup because transactions will not be rolled back and server startup
  could take longer or manually remove ``ib_logfiles`` and
  :file:`xtrabackup_logfile` and run :option:`xtrabackup --prepare` second
  time.
