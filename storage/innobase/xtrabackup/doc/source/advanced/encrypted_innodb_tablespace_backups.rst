.. _encrypted_innodb_tablespace_backups:

===================================
Encrypted InnoDB tablespace backups
===================================

As of |MySQL| 5.7.11, InnoDB supports data `encryption for InnoDB tables <http://dev.mysql.com/doc/refman/5.7/en/innodb-tablespace-encryption.html>`_ stored in file-per-table tablespaces. This feature provides at-rest encryption for physical tablespace data files.

For authenticated user or application to access encrypted tablespace, InnoDB will use master encryption key to decrypt the tablespace key. The master encryption key is stored in a keyring file in the location specified by the :option:`keyring_file_data` configuration option. 

Support for encrypted InnoDB tablespace backups has been implemented in |Percona XtraBackup| 2.4.2 by implementing :option:`xtrabackup --keyring-file-data` option (and also :option:`xtrabackup --server-id` option, needed for |MySQL| prior to 5.7.13). These options are only recognized by |xtrabackup| binary i.e., |innobackupex| will not be able to backup and prepare encrypted tablespaces.

.. contents::
   :local:

Creating Backup
===============

In order to backup and prepare database containing encrypted InnoDB tablespaces, you must specify the path to keyring file by using the :option:`xtrabackup --keyring-file-data` option.

.. code-block:: bash

  xtrabackup --backup --target-dir=/data/backup/ --user=root \
  --keyring-file-data=/var/lib/mysql-keyring/keyring

.. note:: for use with |MySQL| prior to 5.7.13 :option:`--server-id` option is
   added to the backup creation command, making previous example to look like
   the following:

   .. code-block:: bash

      xtrabackup --backup --target-dir=/data/backup/ --user=root \
      --keyring-file-data=/var/lib/mysql-keyring/keyring --server-id=1


After |xtrabackup| is finished taking the backup you should see the following message:

.. code-block:: bash

  xtrabackup: Transaction log of lsn (5696709) to (5696718) was copied.
  160401 10:25:51 completed OK!

.. warning:: 

  |xtrabackup| will not copy keyring file into the backup directory. In order to be prepare the backup, you must make a copy of keyring file yourself. 

Preparing the Backup
====================

In order to prepare the backup you'll need to specify the keyring-file-data (server-id is stored in :file:`backup-my.cnf` file, so it can be omitted when preparing the backup, regardless of the |MySQL| version used). 

.. code-block:: bash

  xtrabackup --prepare --target-dir=/data/backup \
  --keyring-file-data=/var/lib/mysql-keyring/keyring

After |xtrabackup| is finished preparing the backup you should see the following message:

.. code-block:: bash

  InnoDB: Shutdown completed; log sequence number 5697064
  160401 10:34:28 completed OK!

Backup is now prepared and can be restored with :option:`xtrabackup --copy-back` option. In case the keyring has been rotated you'll need to restore the keyring which was used to take and prepare the backup. 

Incremental Encrypted InnoDB tablespace backups
===============================================

The process of taking incremental backups with InnoDB tablespace encryption is similar to taking the :ref:`xb_incremental` with unencrypted tablespace. 

Creating an Incremental Backup
------------------------------

To make an incremental backup, begin with a full backup. The |xtrabackup| binary writes a file called :file:`xtrabackup_checkpoints` into the backup's target directory. This file contains a line showing the ``to_lsn``, which is the database's :term:`LSN` at the end of the backup. First you need to create a full backup with the following command: 

.. code-block:: bash

  xtrabackup --backup --target-dir=/data/backups/base \
  --keyring-file-data=/var/lib/mysql-keyring/keyring

.. warning:: 

  |xtrabackup| will not copy keyring file into the backup directory. In order to be prepare the backup, you must make a copy of keyring file yourself. If you try to restore the backup after the keyring has been changed you'll see errors like ``ERROR 3185 (HY000): Can't find master key from keyring, please check keyring plugin is loaded.`` when trying to access encrypted table.

If you look at the :file:`xtrabackup_checkpoints` file, you should see some contents similar to the following: 

.. code-block:: none

  backup_type = full-backuped
  from_lsn = 0
  to_lsn = 7666625
  last_lsn = 7666634
  compact = 0
  recover_binlog_info = 1

Now that you have a full backup, you can make an incremental backup based on it. Use a command such as the following: 

.. code-block:: bash

   xtrabackup --backup --target-dir=/data/backups/inc1 \
   --incremental-basedir=/data/backups/base \
  --keyring-file-data=/var/lib/mysql-keyring/keyring

.. warning:: 

  |xtrabackup| will not copy keyring file into the backup directory. In order to be prepare the backup, you must make a copy of keyring file yourself. If the keyring hasn't been rotated you can use the same as the one you've backed-up with the base backup. If the keyring has been rotated you'll need to back it up otherwise you won't be able to prepare the backup.

The :file:`/data/backups/inc1/` directory should now contain delta files, such as :file:`ibdata1.delta` and :file:`test/table1.ibd.delta`. These represent the changes since the ``LSN 7666625``. If you examine the :file:`xtrabackup_checkpoints` file in this directory, you should see something similar to the following: 

.. code-block:: none

   backup_type = incremental
   from_lsn = 7666625
   to_lsn = 8873920
   last_lsn = 8873929
   compact = 0
   recover_binlog_info = 1

The meaning should be self-evident. It's now possible to use this directory as the base for yet another incremental backup: 

.. code-block:: bash

   xtrabackup --backup --target-dir=/data/backups/inc2 \
   --incremental-basedir=/data/backups/inc1 \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

Preparing the Incremental Backups
---------------------------------

The :option:`xtrabackup --prepare` step for incremental backups is not the same as for normal backups. In normal backups, two types of operations are performed to make the database consistent: committed transactions are replayed from the log file against the data files, and uncommitted transactions are rolled back. You must skip the rollback of uncommitted transactions when preparing a backup, because transactions that were uncommitted at the time of your backup may be in progress, and it's likely that they will be committed in the next incremental backup. You should use the :option:`xtrabackup --apply-log-only` option to prevent the rollback phase.

.. warning:: 

  If you do not use the :option:`xtrabackup --apply-log-only` option to prevent the rollback phase, then your incremental backups will be useless. After transactions have been rolled back, further incremental backups cannot be applied.

Beginning with the full backup you created, you can prepare it, and then apply the incremental differences to it. Recall that you have the following backups: 

.. code-block:: bash

  /data/backups/base
  /data/backups/inc1
  /data/backups/inc2

To prepare the base backup, you need to run :option:`--prepare` as usual, but prevent the rollback phase: 

.. code-block:: bash

  xtrabackup --prepare --apply-log-only --target-dir=/data/backups/base \
  --keyring-file-data=/var/lib/mysql-keyring/keyring

The output should end with some text such as the following: 

.. code-block:: bash

  InnoDB: Shutdown completed; log sequence number 7666643
  InnoDB: Number of pools: 1
  160401 12:31:11 completed OK!

To apply the first incremental backup to the full backup, you should use the following command: 

.. code-block:: bash

  xtrabackup --prepare --apply-log-only --target-dir=/data/backups/base \
  --incremental-dir=/data/backups/inc1 \
  --keyring-file-data=/var/lib/mysql-keyring/keyring

.. warning::

  Backup should be prepared with the keyring that was used when backup was being taken. This means that if the keyring has been rotated between the base and incremental backup that you'll need to use the keyring that was in use when the first incremental backup has been taken.

Preparing the second incremental backup is a similar process: apply the deltas to the (modified) base backup, and you will roll its data forward in time to the point of the second incremental backup: 

.. code-block:: bash

  xtrabackup --prepare --target-dir=/data/backups/base \
  --incremental-dir=/data/backups/inc2 \
  --keyring-file-data=/var/lib/mysql-keyring/keyring

.. note::
     
  :option:`xtrabackup --apply-log-only` should be used when merging all incrementals except the last one. That's why the previous line doesn't contain the :option:`--apply-log-only` option. Even if the :option:`--apply-log-only` was used on the last step, backup would still be consistent but in that case server would perform the rollback phase.

Backup is now prepared and can be restored with :option:`xtrabackup --copy-back` option. In case the keyring has been rotated you'll need to restore the keyring which was used to take and prepare the backup.

