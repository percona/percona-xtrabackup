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

  xtrabackup_56 --backup --target-dir=/data/backup/ --datadir=/var/lib/mysql/

If you want a usable full backup, use :doc:`innobackupex <../innobackupex/innobackupex_script>` since `xtrabackup` itself won't copy table definitions, triggers, or anything else that's not .ibd.

If you look at the :file:`xtrabackup_checkpoints` file, you should see some contents similar to the following: ::

  backup_type = full-backuped
  from_lsn = 0
  to_lsn = 1546908388
  last_lsn = 1574827339
  compact = 0

Using the Log Archiving to prepare the backup
=============================================

In order to prepare the backup we need to specify the directory that contains the archived logs with the :option:`xtrabackup --innodb-log-arch-dir` option: ::

 xtrabackup_56 --prepare --target-dir=/data/backup/ --innodb-log-arch-dir=/data/archived-logs/

This command will use archived logs, replay them against the backup folder and bring them up to date with the latest archived log.

Output should look like this: ::

  xtrabackup_56 version 2.1.5 for MySQL server 5.6.11 Linux (x86_64) (revision id: 680)
  xtrabackup: cd to /tmp/backup-01/
  xtrabackup: using the following InnoDB configuration for recovery:
  xtrabackup:   innodb_data_home_dir = ./
  xtrabackup:   innodb_data_file_path = ibdata1:10M:autoextend
  xtrabackup:   innodb_log_group_home_dir = ./
  xtrabackup:   innodb_log_files_in_group = 2
  xtrabackup:   innodb_log_file_size = 50331648
  InnoDB: Allocated tablespace 4, old maximum was 0
  xtrabackup: using the following InnoDB configuration for recovery:
  xtrabackup:   innodb_data_home_dir = ./
  xtrabackup:   innodb_data_file_path = ibdata1:10M:autoextend
  xtrabackup:   innodb_log_group_home_dir = ./
  xtrabackup:   innodb_log_files_in_group = 2
  xtrabackup:   innodb_log_file_size = 50331648
  xtrabackup: Starting InnoDB instance for recovery.
  xtrabackup: Using 104857600 bytes for buffer pool (set by --use-memory parameter)
  InnoDB: The InnoDB memory heap is disabled
  InnoDB: Mutexes and rw_locks use GCC atomic builtins
  InnoDB: Compressed tables use zlib 1.2.3
  InnoDB: Not using CPU crc32 instructions
  InnoDB: Initializing buffer pool, size = 100.0M
  InnoDB: Completed initialization of buffer pool
  InnoDB: Setting log file ./ib_logfile101 size to 48 MB
  InnoDB: Setting log file ./ib_logfile1 size to 48 MB
  InnoDB: Renaming log file ./ib_logfile101 to ./ib_logfile0
  InnoDB: New log files created, LSN=1627148
  InnoDB:  Starting archive recovery from a backup...
  InnoDB: Allocated tablespace 4, old maximum was 0
  InnoDB: Opened archived log file /var/lib/mysql/ib_log_archive_00000000000000045568
  InnoDB: Starting an apply batch of log records to the database...
  InnoDB: Progress in percent: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95 96 97 98 99 

  ...

  InnoDB: Apply batch completed
  InnoDB: Starting an apply batch of log records to the database...
  InnoDB: Progress in percent: 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95 96 97 98 99 
  InnoDB: Apply batch completed
  InnoDB: 1 transaction(s) which must be rolled back or cleaned up
  InnoDB: in total 5063 row operations to undo
  InnoDB: Trx id counter is 4096

  [notice (again)]
  If you use binary log and don't use any hack of group commit,
  the binary log position seems to be:

  xtrabackup: starting shutdown with innodb_fast_shutdown = 1
  InnoDB: Starting shutdown...
  InnoDB: Shutdown completed; log sequence number 2013229561

After this is completed successfully backup can be restored.

You can check the :file:`xtrabackup_checkpoints` file and see that the backup_type has changed: ::

   backup_type = full-prepared
   from_lsn = 0
   to_lsn = 1546908388
   last_lsn = 1574827339
   compact = 0

.. note:: 

   Archived logs can be applied to backup data several times, for example to decrease the backup size or time required for preparing the backup.

Additional option is available if you need to restore a backup to specific point in time. By adding the :option:`xtrabackup --to-archived-lsn` option you can specify the LSN to which the backup will be prepared. ::

 xtrabackup_56 --prepare --target-dir=/data/backup/ --innodb-log-arch-dir=/data/archived-logs/ --to-archived-lsn=5536301566

This will prepare the backup up to the specified Log Sequence Number.
 
