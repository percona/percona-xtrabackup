.. _xb_incremental_ps_56:

===================================================
 Incremental Backups with Log Archiving for XtraDB
===================================================

|Percona Server| 5.6.11-60.3 has introduced a new feature called `Log Archiving for XtraDB <http://www.percona.com/doc/percona-server/5.6/management/log_archiving.html>`_. This feature makes copies of the old log files before they are overwritten, thus saving all the redo log for a write workload.

When log archiving is enabled, it duplicates all redo log writes in a separate set of files in addition to normal redo log writing, creating new files as necessary.

Archived log file name format is ``ib_log_archive_<startlsn>``. The start LSN marks the log sequence number when the archive was started. An example of the archived log files should look like this: :: 

 ib_log_archive_00000000010145937920
 ib_log_archive_00000000010196267520

In order to enable this feature, variable :option:`innodb_log_archive` should be set to ``ON``. Once the feature has been enabled, directory specified with :option:`innodb_log_arch_dir` (|MySQL| datadir by default) will contain the archived log files. 

Creating the Backup
===================

To make an incremental backup, begin with a full backup as usual. The |xtrabackup| binary writes a file called :file:`xtrabackup_checkpoints` into the backup's target directory. This file contains a line showing the ``to_lsn``, which is the database's :term:`LSN` at the end of the backup. :doc:`Create the full backup <creating_a_backup>` with a command such as the following: ::

  xtrabackup --backup --target-dir=/data/backup/ --datadir=/var/lib/mysql/

If you want a usable full backup, use :doc:`innobackupex <../innobackupex/innobackupex_script>` since `xtrabackup` itself won't copy table definitions, triggers, or anything else that's not .ibd.

If you look at the :file:`xtrabackup_checkpoints` file, you should see some contents similar to the following: ::

  backup_type = full-backuped
  from_lsn = 0
  to_lsn = 2066277743
  last_lsn = 2066277743
  compact = 0

Using the Log Archiving to prepare the backup
=============================================

In order to prepare the backup we need to specify the directory that contains the archived logs with the :option:`xtrabackup --archived-logs-dir` option: ::

 xtrabackup --prepare --target-dir=/data/backup/ --archived-logs-dir=/data/archived-logs/

This command will use archived logs, replay them against the backup folder and bring them up to date with the latest archived log.

Output should look like this: ::

 InnoDB: Doing recovery: scanned up to log sequence number 712612864 (0 %)
 InnoDB: Doing recovery: scanned up to log sequence number 717855744 (0 %)
 InnoDB: Doing recovery: scanned up to log sequence number 723098624 (1 %)
 InnoDB: Doing recovery: scanned up to log sequence number 728341504 (1 %)
 ...
 InnoDB: Apply batch completed
 InnoDB: Doing recovery: scanned up to log sequence number 2066277743 (99 %)
 130919  8:12:44  InnoDB: Starting an apply batch of log records to the database...
 InnoDB: Progress in percents: 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95 96 97 98 99 
 InnoDB: Apply batch completed
 130919  8:12:45 Percona XtraDB (http://www.percona.com) 5.1.70-14.6 started; log sequence number 2066277743

After this is completed successfully backup can be restored.

.. note:: 

   Archived logs can be applied to backup data several times, for example to decreasing backup size or time required for preparing the backup.

Additional option is available if you need to restore a backup to specific point in time. By adding the :option:`xtrabackup --to-archived-lsn` option you can specify the LSN to which the backup will be prepared. ::

 xtrabackup --prepare --target-dir=/data/backup/ --archived-logs-dir=/data/archived-logs/ --to-archived-lsn=8155020800

This will prepare the backup up to the specified Log Sequence Number.
 
