.. _encrypted_innodb_tablespace_backups:

================================================================================
Encrypted InnoDB Tablespace Backups
================================================================================

As of |MySQL| 5.7.11, InnoDB supports data `encryption for InnoDB tables
<http://dev.mysql.com/doc/refman/5.7/en/innodb-tablespace-encryption.html>`_
stored in file-per-table tablespaces. This feature provides at-rest encryption
for physical tablespace data files.

For authenticated user or application to access encrypted tablespace, InnoDB
will use master encryption key to decrypt the tablespace key. The master
encryption key is stored in a keyring. Two keyring plugins supported by
|xtrabackup| are ``keyring_file`` and ``keyring_vault``. These plugins are
installed into the plugin directory.

.. contents::
   :local:

Making a Backup Using ``keyring_file`` Plugin
================================================================================

Support for encrypted InnoDB tablespace backups with ``keyring_file`` has been
implemented in |Percona XtraBackup| 2.4.2 by implementing :option:`xtrabackup
--keyring-file-data` option (and also :option:`xtrabackup --server-id` option,
needed for |MySQL| prior to 5.7.13). These options are only recognized by
|xtrabackup| binary i.e., |innobackupex| will not be able to backup and prepare
encrypted tablespaces.

Creating Backup
--------------------------------------------------------------------------------

In order to backup and prepare database containing encrypted InnoDB tablespaces,
you must specify the path to keyring file by using the :option:`xtrabackup
--keyring-file-data` option.

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/data/backup/ --user=root \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

With |MySQL| prior to 5.7.13, use :option:`xtrabackup --server-id` in the backup
creation command:

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/data/backup/ --user=root \
   --keyring-file-data=/var/lib/mysql-keyring/keyring --server-id=1

After |xtrabackup| is finished taking the backup you should see the following
message:

.. code-block:: bash

   xtrabackup: Transaction log of lsn (5696709) to (5696718) was copied.
   160401 10:25:51 completed OK!

.. warning:: 

  |xtrabackup| will not copy keyring file into the backup directory. In order to
  be prepare the backup, you must make a copy of keyring file yourself.

Preparing Backup
--------------------------------------------------------------------------------

In order to prepare the backup you'll need to specify the keyring-file-data
(server-id is stored in :file:`backup-my.cnf` file, so it can be omitted when
preparing the backup, regardless of the |MySQL| version used).

.. code-block:: bash

  xtrabackup --prepare --target-dir=/data/backup \
  --keyring-file-data=/var/lib/mysql-keyring/keyring

After |xtrabackup| is finished preparing the backup you should see the following
message:

.. code-block:: bash

  InnoDB: Shutdown completed; log sequence number 5697064
  160401 10:34:28 completed OK!

Backup is now prepared and can be restored with :option:`xtrabackup --copy-back`
option. In case the keyring has been rotated you'll need to restore the keyring
which was used to take and prepare the backup.

Making Backup Using ``keyring_vault`` Plugin
================================================================================

Support for encrypted InnoDB tablespace backups with ``keyring_vault`` has been
implemented in |Percona XtraBackup| 2.4.11.  Keyring vault plugin settings are
described `here
<https://www.percona.com/doc/percona-server/LATEST/management/data_at_rest_encryption.html>`_.

Creating Backup
--------------------------------------------------------------------------------

The following command creates a backup in the ``/data/backup`` directory:

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/data/backup --user=root 

After |xtrabackup| completes taking the backup you should see the following
message:

.. code-block:: bash

   xtrabackup: Transaction log of lsn (5696709) to (5696718) was copied.
   160401 10:25:51 completed OK!

Preparing the Backup
--------------------------------------------------------------------------------

In order to prepare the backup |xtrabackup| will need an access to the keyring.
Since |xtrabackup| doesn't talk to MySQL server and doesn't read default
``my.cnf`` configuration file during prepare, user will need to specify keyring
settings via the command line:

.. code-block:: bash

   $ xtrabackup --prepare --target-dir=/data/backup \
   --keyring-vault-config=/etc/vault.cnf

.. seealso::

   Data at Rest Encryption for Percona Server
      `keyring vault plugin settings <https://www.percona.com/doc/percona-server/LATEST/management/data_at_rest_encryption.html#keyring-vault-plugin>`_

After |xtrabackup| completes preparing the backup you should see the following
message:

.. code-block:: bash

   InnoDB: Shutdown completed; log sequence number 5697064
   160401 10:34:28 completed OK!

The backup is now prepared and can be restored with :option:`xtrabackup
--copy-back` option:

.. code-block:: bash

   xtrabackup --copy-back --target-dir=/data/backup --datadir=/data/mysql

Incremental Encrypted InnoDB Tablespace Backups with ``keyring_file``
================================================================================

The process of taking incremental backups with InnoDB tablespace encryption is
similar to taking the :ref:`xb_incremental` with unencrypted tablespace.

Creating an Incremental Backup
--------------------------------------------------------------------------------

To make an incremental backup, begin with a full backup. The |xtrabackup| binary
writes a file called :file:`xtrabackup_checkpoints` into the backup's target
directory. This file contains a line showing the ``to_lsn``, which is the
database's :term:`LSN` at the end of the backup. First you need to create a full
backup with the following command:

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/data/backups/base \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

.. warning:: 

   |xtrabackup| will not copy keyring file into the backup directory. In order to
   be prepare the backup, you must make a copy of keyring file yourself. If you
   try to restore the backup after the keyring has been changed you'll see errors
   like ``ERROR 3185 (HY000): Can't find master key from keyring, please check
   keyring plugin is loaded.`` when trying to access encrypted table.

If you look at the :file:`xtrabackup_checkpoints` file, you should see some
contents similar to the following:

.. code-block:: none

   backup_type = full-backuped
   from_lsn = 0
   to_lsn = 7666625
   last_lsn = 7666634
   compact = 0
   recover_binlog_info = 1

Now that you have a full backup, you can make an incremental backup based on
it. Use a command such as the following:

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/data/backups/inc1 \
   --incremental-basedir=/data/backups/base \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

.. warning:: 

   |xtrabackup| will not copy keyring file into the backup directory. In order to
   be prepare the backup, you must make a copy of keyring file yourself. If the
   keyring hasn't been rotated you can use the same as the one you've backed-up
   with the base backup. If the keyring has been rotated you'll need to back it
   up otherwise you won't be able to prepare the backup.

The :file:`/data/backups/inc1/` directory should now contain delta files, such
as :file:`ibdata1.delta` and :file:`test/table1.ibd.delta`. These represent the
changes since the ``LSN 7666625``. If you examine the
:file:`xtrabackup_checkpoints` file in this directory, you should see something
similar to the following:

.. code-block:: none

   backup_type = incremental
   from_lsn = 7666625
   to_lsn = 8873920
   last_lsn = 8873929
   compact = 0
   recover_binlog_info = 1

The meaning should be self-evident. It's now possible to use this directory as
the base for yet another incremental backup:

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/data/backups/inc2 \
   --incremental-basedir=/data/backups/inc1 \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

Preparing the Incremental Backups
--------------------------------------------------------------------------------

The :option:`xtrabackup --prepare` step for incremental backups is not the same
as for normal backups. In normal backups, two types of operations are performed
to make the database consistent: committed transactions are replayed from the
log file against the data files, and uncommitted transactions are rolled
back. You must skip the rollback of uncommitted transactions when preparing a
backup, because transactions that were uncommitted at the time of your backup
may be in progress, and it's likely that they will be committed in the next
incremental backup. You should use the :option:`xtrabackup --apply-log-only`
option to prevent the rollback phase.

.. warning:: 

  If you do not use the :option:`xtrabackup --apply-log-only` option to prevent
  the rollback phase, then your incremental backups will be useless. After
  transactions have been rolled back, further incremental backups cannot be
  applied.

Beginning with the full backup you created, you can prepare it, and then apply
the incremental differences to it. Recall that you have the following backups:

.. code-block:: bash

   /data/backups/base
   /data/backups/inc1
   /data/backups/inc2

To prepare the base backup, you need to run :option:`xtrabackup --prepare` as
usual, but prevent the rollback phase:

.. code-block:: bash

   $ xtrabackup --prepare --apply-log-only --target-dir=/data/backups/base \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

The output should end with some text such as the following: 

.. code-block:: bash

   InnoDB: Shutdown completed; log sequence number 7666643
   InnoDB: Number of pools: 1
   160401 12:31:11 completed OK!

To apply the first incremental backup to the full backup, you should use the following command: 

.. code-block:: bash

   $ xtrabackup --prepare --apply-log-only --target-dir=/data/backups/base \
   --incremental-dir=/data/backups/inc1 \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

.. warning::

   Backup should be prepared with the keyring that was used when backup was
   being taken. This means that if the keyring has been rotated between the base
   and incremental backup that you'll need to use the keyring that was in use
   when the first incremental backup has been taken.

Preparing the second incremental backup is a similar process: apply the deltas
to the (modified) base backup, and you will roll its data forward in time to the
point of the second incremental backup:

.. code-block:: bash

   $ xtrabackup --prepare --target-dir=/data/backups/base \
   --incremental-dir=/data/backups/inc2 \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

.. note::
     
   :option:`xtrabackup --apply-log-only` should be used when merging all
   incrementals except the last one. That's why the previous line doesn't
   contain :option:`xtrabackup --apply-log-only`. Even if the
   :option:`xtrabackup --apply-log-only` were used on the last step, backup
   would still be consistent but in that case server would perform the rollback
   phase.

The backup is now prepared and can be restored with :option:`xtrabackup
--copy-back`. In case the keyring has been rotated you'll need to restore the
keyring which was used to take and prepare the backup.

Restoring backup when keyring is not available
================================================================================

While described restore method works, it requires an access to the same keyring
which server is using. It may not be possible if backup is prepared on different
server or at the much later time, when keys in the keyring have been purged, or
in case of malfunction when keyring vault server is not available at all.

A :option:`xtrabackup --transition-key` should be used to make it possible
for |xtrabackup| to process the backup without access to the keyring vault
server. In this case |xtrabackup| will derive AES encryption key from specified
passphrase and will use it to encrypt tablespace keys of tablespaces being
backed up.

Creating the Backup with a Passphrase
--------------------------------------------------------------------------------

The following example illustrates how a backup can be created in this case:

.. code-block:: bash

   $ xtrabackup --backup --user=root -p --target-dir=/data/backup \
   --transition-key=MySecetKey

If :option:`xtrabackup --transition-key` is specified without a value,
xtrabackup will ask for it.

.. note::

   :option:`xtrabackup --transition-key` scrapes the supplkied value so that it
   should not visible in the ``ps`` command output.

Preparing the Backup with a Passphrase
--------------------------------------------------------------------------------

The same passphrase should be specified for the ``prepare`` command:

.. code-block:: bash

   $ xtrabackup --prepare --target-dir=/data/backup

There is no ``keyring-vault`` or ``keyring-file`` here, because |xtrabackup|
does not talk to the keyring in this case.

Restoring the Backup with a Generated Key
--------------------------------------------------------------------------------

When restoring a backup you will need to generate new master key. Here is the
example for ``keyring_file``:

.. code-block:: bash

   $ xtrabackup --copy-back --target-dir=/data/backup --datadir=/data/mysql \
   --transition-key=MySecetKey --generate-new-master-key \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

In case of ``keyring_vault`` it will look like this:

.. code-block:: bash

   $ xtrabackup --copy-back --target-dir=/data/backup --datadir=/data/mysql \
   --transition-key=MySecetKey --generate-new-master-key \
   --keyring-vault-config=/etc/vault.cnf

|xtrabackup| will generate new master key, store it into target keyring vault
server and re-encrypt tablespace keys using this key.

Making the Backup with a Stored Transition Key
================================================================================

Finally, there is an option to store transition key in the keyring. In this case
|xtrabackup| will need an access to the same keyring file or vault server during
prepare and copy-back, but does not depend on whether the server keys have been
purged.

In this scenario, the three stages of the backup process are the following:

.. contents::
   :local:      

Backup
--------------------------------------------------------------------------------

.. code-block:: bash

   $ xtrabackup --backup --user=root -p --target-dir=/data/backup \
   --generate-transition-key

Prepare
--------------------------------------------------------------------------------

.. rubric:: ``keyring_file`` variant

.. code-block:: bash

   $ xtrabackup --prepare --target-dir=/data/backup \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

.. rubric:: ``keyring_vault`` variant

.. code-block:: bash

   $ xtrabackup --prepare --target-dir=/data/backup \
   --keyring-vault-config=/etc/vault.cnf

Copy-back
--------------------------------------------------------------------------------

.. rubric:: ``keyring_file`` variant

.. code-block:: bash

   $ xtrabackup --copy-back --target-dir=/data/backup --datadir=/data/mysql \
   --generate-new-master-key --keyring-file-data=/var/lib/mysql-keyring/keyring

.. rubric:: ``keyring_vault`` variant

.. code-block:: bash

   $ xtrabackup --copy-back --target-dir=/data/backup --datadir=/data/mysql \
   --generate-new-master-key --keyring-vault-config=/etc/vault.cnf

