.. _parallel-ibk:

=================================
 Accelerating the backup process 
=================================

Accelerating with :option:`innobackupex --parallel` copy and `--compress-threads`
----------------------------------------------------------------------------------------------------

When performing a local backup or the streaming backup with |xbstream| option, multiple files can be copied concurrently by using the :option:`innobackupex --parallel` option. This option specifies the number of threads created by |xtrabackup| to copy data files.

To take advantage of this option either the multiple tablespaces option must be enabled (:term:`innodb_file_per_table`) or the shared tablespace must be stored in multiple :term:`ibdata` files with the :term:`innodb_data_file_path` option.  Having multiple files for the database (or splitting one into many) doesn't have a measurable impact on performance.


As this feature is implemented **at a file level**, concurrent file transfer can sometimes increase I/O throughput when doing a backup on highly fragmented data files, due to the overlap of a greater number of random read requests. You should consider tuning the filesystem also to obtain the maximum performance (e.g. checking fragmentation). 

If the data is stored on a single file, this option will have no effect.

To use this feature, simply add the option to a local backup, for example: ::

  $ innobackupex --parallel=4 /path/to/backup

By using the |xbstream| in streaming backups you can additionally speed up the compression process by using the :option:`innobackupex --compress-threads` option. This option specifies the number of threads created by |xtrabackup| for  for parallel data compression. The default value for this option is 1.

To use this feature, simply add the option to a local backup, for example ::

 $ innobackupex --stream=xbstream --compress --compress-threads=4 ./ > backup.xbstream 

Before applying logs, compressed files will need to be uncompressed.

Accelerating with :option:`innobackupex --rsync`
--------------------------------------------------------------------------------

In order to speed up the backup process and to minimize the time ``FLUSH TABLES WITH READ LOCK`` is blocking the writes, option :option:`innobackupex --rsync` should be used. When this option is specified, |innobackupex| uses ``rsync`` to copy all non-InnoDB files instead of spawning a separate ``cp`` for each file, which can be much faster for servers with a large number of databases or tables. |innobackupex| will call the ``rsync`` twice, once before the ``FLUSH TABLES WITH READ LOCK`` and once during to minimize the time the read lock is being held. During the second ``rsync`` call, it will only synchronize the changes to non-transactional data (if any) since the first call performed before the ``FLUSH TABLES WITH READ LOCK``. Note that |Percona XtraBackup| will use `Backup locks <https://www.percona.com/doc/percona-server/5.6/management/backup_locks.html#backup-locks>`_ where available as a lightweight alternative to ``FLUSH TABLES WITH READ LOCK``. This feature is available in |Percona Server| 5.6+. |Percona XtraBackup| uses this automatically to copy non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.

.. note::
 
 This option cannot be used together with the :option:`innobackupex --stream` option.

