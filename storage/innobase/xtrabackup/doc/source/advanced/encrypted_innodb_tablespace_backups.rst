.. _encrypted_innodb_tablespace_backups:

===================================
Encrypted InnoDB tablespace backups
===================================

InnoDB supports `data encryption for InnoDB tables
<https://dev.mysql.com/doc/refman/8.0/en/innodb-data-encryption.html>`__ stored in file-per-table tablespaces. This feature provides an at-rest encryption for physical tablespace data files.

For an authenticated user or application to access an encrypted tablespace,
InnoDB uses the master encryption key to decrypt the tablespace key. The
master encryption key is stored in a keyring. *xtrabackup* supports two keyring
plugins: ``keyring_file``, and ``keyring_vault``. These plugins are installed
into the ``plugin`` directory.

.. rubric:: Version Updates

Percona XtraBackup 8.0.25-17 adds support for the ``keyring_file`` component, which is part of the component-based infrastructure MySQL which extends the server capabilities. The component is stored in the ``plugin`` directory. 

See a `comparison of keyring components and keyring plugins <https://dev.mysql.com/doc/refman/8.0/en/keyring-component-plugin-comparison.html>`__ for more information.

Percona XtraBackup 8.0.27-19 adds support for the Key Management Interoperability Protocol (KMIP) which enables the communication between the key management system and encrypted database server. This feature is *tech preview* quality.

Percona XtraBackup 8.0.28-21 adds support for the Amazon Key Management Service (AWS KMS). AWS KMS is cloud-based encryption and key management service. The keys and functionality can be used for other AWS services or your applications that use AWS. No configuration is required to back up a server with AWS KMS-enabled encryption. This feature is *tech preview* quality.

.. contents::
   :local:

Making a backup
================

Using ``keyring_file`` plugin
-----------------------------

In order to backup and prepare a database containing encrypted InnoDB
tablespaces, specify the path to a keyring file as the value of the
``--keyring-file-data`` option.

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/data/backup/ --user=root \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

After *xtrabackup* takes the backup, the following
message confirms the action:

.. code-block:: bash

   xtrabackup: Transaction log of lsn (5696709) to (5696718) was copied.
   160401 10:25:51 completed OK!

.. warning:: 

   *xtrabackup* does not copy the keyring file into the backup directory. To prepare the backup, you must copy the keyring file manually.

.. rubric:: Preparing the Backup

To prepare the backup specify the keyring-file-data.

.. code-block:: bash

   $ xtrabackup --prepare --target-dir=/data/backup \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

After *xtrabackup* takes the backup, the following
message confirms the action:

.. code-block:: bash

   InnoDB: Shutdown completed; log sequence number 5697064
   160401 10:34:28 completed OK!

The backup is now prepared and can be restored with the :option:`--copy-back`
option. In case the keyring has been rotated, you must restore the keyring used when the backup was  taken and prepared.

Using ``keyring_vault`` plugin
------------------------------

Keyring vault plugin settings are
described `here
<https://www.percona.com/doc/percona-server/LATEST/security/using-keyring-plugin.html#using-keyring-plugin>`_.

.. rubric:: Creating Backup

The following command creates a backup in the ``/data/backup`` directory:

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/data/backup --user=root 


After *xtrabackup* completes the action, the following confirmation message appears:

.. code-block:: bash

   xtrabackup: Transaction log of lsn (5696709) to (5696718) was copied.
   160401 10:25:51 completed OK!

.. rubric:: Preparing the Backup

To prepare the backup, *xtrabackup* must access the keyring.
*xtrabackup* does not communicate with the *MySQL* server or read the default ``my.cnf`` configuration file. Specify the keyring settings in the command line:

.. code-block:: bash

   $ xtrabackup --prepare --target-dir=/data/backup \
   --keyring-vault-config=/etc/vault.cnf

.. note::

   Please look `here
   <https://www.percona.com/doc/percona-server/LATEST/security/using-keyring-plugin.html#using-keyring-plugin>`_
   for a description of keyring vault plugin settings.

After *xtrabackup* completes the action, the following confirmation message appears:

.. code-block:: text

   InnoDB: Shutdown completed; log sequence number 5697064
   160401 10:34:28 completed OK!

The backup is now prepared and can be restored with the :option:`--copy-back` option:

.. code-block:: bash

   $ xtrabackup --copy-back --target-dir=/data/backup --datadir=/data/mysql


Using the ``keyring_file`` component
-------------------------------------

A component is not loaded with the ``--early_plugin_load`` option. The server uses a manifest to load the component and the component has its own configuration file. See `component installation <https://dev.mysql.com/doc/refman/8.0/en/keyring-component-installation.html>`__ for more information.

An example of a manifest and a configuration file follows:

./bin/mysqld.my:

.. code-block:: json

   { 
      "components": "file://component_keyring_file" 
   }

/lib/plugin/component_keyring_file.cnf:

.. code-block:: json

   { 
      "path": "/var/lib/mysql-keyring/keyring_file", "read_only": false 
   }


For more information, see `Keyring Component Installation <https://dev.mysql.com/doc/refman/8.0/en/keyring-component-installation.html>`__ and `Using the keyring_file File-Based Keyring Plugin <https://dev.mysql.com/doc/refman/8.0/en/keyring-file-plugin.html>`__.

With the appropriate privilege, you can ``SELECT`` on the `performance_schema.keyring_component_status table <https://dev.mysql.com/doc/refman/8.0/en/performance-schema-keyring-component-status-table.html>`__  to view the attributes and status of the installed keyring component when in use. 

The component has no special requirements for backing up a database that contains encrypted InnoDB tablespaces. 

.. sourcecode:: bash

   xtrabackup --backup --target-dir=/data/backup --user=root

After *xtrabackup* completes the action, the following confirmation message appears:

.. sourcecode:: bash

   xtrabackup: Transaction log of lsn (5696709) to (5696718) was copied.
   160401 10:25:51 completed OK!

.. warning:: 

   *xtrabackup* does not copy the keyring file into the backup directory. To prepare the backup, you must copy the keyring file manually.

.. rubric:: Preparing the Backup

*xtrabackup* reads the keyring_file component configuration from ``xtrabackup_component_keyring_file.cnf``. You must specify the keyring_file data path if the ``keyring-file-data`` is not located in the attribute ``PATH`` from the xtrabackup_component_keyring_file.cnf. 

The following is an example of adding the location for the keyring-file-data:

.. sourcecode:: bash

   xtrabackup --prepare --target-dir=/data/backup \ 
   --keyring-file-data=/var/lib/mysql-keyring/keyring

.. note:: *xtrabackup* attempts to read ``xtrabackup_component_keyring_file.cnf``. You can assign another keyring file component configuration by passing the ``--component-keyring-file-config`` option. 

After *xtrabackup* completes preparing the backup, the following confirmation message appears:

.. sourcecode:: bash

   InnoDB: Shutdown completed; log sequence number 5697064
   160401 10:34:28 completed OK!

The backup is prepared. To restore the backup use the ``--copy-back`` option. If the keyring has been rotated, you must restore the specific keyring used to take and prepare the backup.


Incremental Encrypted InnoDB tablespace backups with ``keyring_file``
---------------------------------------------------------------------

The process of taking incremental backups with InnoDB tablespace encryption is
similar to taking the :ref:`xb_incremental` with unencrypted tablespace.

.. note:: The ``keyring-file`` component should not used in production or for regulatory compliance. 

.. rubric:: Creating an Incremental Backup

To make an incremental backup, begin with a full backup. The *xtrabackup* binary
writes a file called :file:`xtrabackup_checkpoints` into the backup's target
directory. This file contains a line showing the ``to_lsn``, which is the
database's :term:`LSN` at the end of the backup. First you need to create a full
backup with the following command:

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/data/backups/base \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

.. warning:: 

   *xtrabackup* will not copy the keyring file into the backup directory. In order to
   prepare the backup, you must make a copy of the keyring file yourself. If you
   try to restore the backup after the keyring has been changed you'll see errors
   like ``ERROR 3185 (HY000): Can't find master key from keyring, please check
   keyring plugin is loaded.`` when trying to access an encrypted table.

If you look at the :file:`xtrabackup_checkpoints` file, you should see
contents similar to the following:

.. code-block:: none

   backup_type = full-backuped
   from_lsn = 0
   to_lsn = 7666625
   last_lsn = 7666634
   compact = 0
   recover_binlog_info = 1

Now that you have a full backup, you can make an incremental backup based on it. Use a command such as the following: 

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/data/backups/inc1 \
   --incremental-basedir=/data/backups/base \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

.. warning:: 

   *xtrabackup* does not copy the keyring file into the backup directory. To prepare the backup, you must copy the keyring file manually. 
   
   If the
   keyring has not been rotated you can use the same as the one you've backed-up
   with the base backup. If the keyring has been rotated or you have upgraded the plugin to a component, you'll need to back up the keyring file,
   otherwise, you are unable to prepare the backup.

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

You can use this directory as the base for yet another incremental backup:

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/data/backups/inc2 \
   --incremental-basedir=/data/backups/inc1 \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

.. rubric:: Preparing Incremental Backups

The :option:`--prepare` step for incremental backups is not the same as for
normal backups. In normal backups, two types of operations are performed to make
the database consistent: committed transactions are replayed from the log file
against the data files, and uncommitted transactions are rolled back. You must
skip the rollback of uncommitted transactions when preparing a backup, because
transactions that were uncommitted at the time of your backup may be in
progress, and it's likely that they will be committed in the next incremental
backup. You should use the :option:`--apply-log-only` option to prevent the
rollback phase.

.. warning:: 

   If you do not use the :option:`--apply-log-only` option to prevent the
   rollback phase, then your incremental backups are useless. After
   transactions have been rolled back, further incremental backups cannot be
   applied.

Beginning with the full backup you created, you can prepare it and then apply
the incremental differences to it. Recall that you have the following backups:

.. code-block:: bash

   /data/backups/base
   /data/backups/inc1
   /data/backups/inc2

To prepare the base backup, you need to run :option:`--prepare` as usual, but
prevent the rollback phase:

.. code-block:: bash

   $ xtrabackup --prepare --apply-log-only --target-dir=/data/backups/base \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

The output should end with some text such as the following: 

.. code-block:: bash

   InnoDB: Shutdown completed; log sequence number 7666643
   InnoDB: Number of pools: 1
   160401 12:31:11 completed OK!

To apply the first incremental backup to the full backup, you should use the
following command:

.. code-block:: bash

   $ xtrabackup --prepare --apply-log-only --target-dir=/data/backups/base \
   --incremental-dir=/data/backups/inc1 \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

.. warning::

   The backup should be prepared with the keyring file and type that was used when backup was being
   taken. This means that if the keyring has been rotated or you have upgraded from a plugin to a component between the base and
   incremental backup that you must use the keyring that was in use when
   the first incremental backup has been taken.

Preparing the second incremental backup is a similar process: apply the deltas
to the (modified) base backup, and you will roll its data forward in time to the
point of the second incremental backup:

.. code-block:: bash

   $ xtrabackup --prepare --target-dir=/data/backups/base \
   --incremental-dir=/data/backups/inc2 \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

.. note::
     
   :option:`--apply-log-only` should be used when merging all
   incrementals except the last one. That's why the previous line doesn't contain
   the :option:`--apply-log-only` option. Even if the :option:`--apply-log-only`
   was used on the last step, backup would still be consistent but in that case
   server would perform the rollback phase.

The backup is now prepared and can be restored with :option:`--copy-back` option. In
case the keyring has been rotated you'll need to restore the keyring which was
used to take and prepare the backup.

Using Key Management Interoperability Protocol
==============================================

This feature is *tech preview* quality.

Percona XtraBackup 8.0.27-19 adds support for the Key Management Interoperability Protocol (KMIP) which enables the communication between the key management system and encrypted database server.

Percona XtraBackup has no special requirements for backing up a database that contains encrypted InnoDB tablespaces. 

Percona XtraBackup performs the following actions:

1. Connects to the MySQL server
2. Pulls the configuration
3. Connects to the KMIP server
4. Fetches the necessary keys from the KMIP server
5. Stores the KMIP server configuration settings in the ``xtrabackup_component_keyring_kmip.cnf`` file in the backup directory

When preparing the backup, Percona XtraBackup connects to the KMIP server with the settings from the ``xtrabackup_component_keyring_kmip.cnf`` file.
   
Restoring a Backup When Keyring Is not Available
================================================================================

While the described restore method works, this method requires access to the same
keyring that the server is using. It may not be possible if the backup is prepared
on a different server or at a much later time, when keys in the keyring are
purged, or, in the case of a malfunction, when the keyring vault server is not
available at all.

The ``--transition-key=<passphrase>`` option should be used to make it possible
for *xtrabackup* to process the backup without access to the keyring vault
server. In this case, *xtrabackup* derives the AES encryption key from the
specified passphrase and will use it to encrypt tablespace keys of tablespaces
that are being backed up.

.. rubric:: Creating a Backup with a Passphrase

The following example illustrates how the backup can be created in this case:

.. code-block:: bash

   $ xtrabackup --backup --user=root -p --target-dir=/data/backup \
   --transition-key=MySecetKey

If ``--transition-key`` is specified without a value, *xtrabackup* will ask for
it.

.. note::

   *xtrabackup* scrapes ``--transition-key`` so that its value is not visible in
   the ``ps`` command output.

.. rubric:: Preparing the Backup with a Passphrase

The same passphrase should be specified for the `prepare` command:

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/data/backup \
   --transition-key=MySecretKey

There are no ``--keyring-vault...``,``--keyring-file...``, or ``--component-keyring-file-config`` options here,
because *xtrabackup* does not talk to the keyring in this case.

.. rubric:: Restoring the Backup with a Generated Key

When restoring a backup you will need to generate a new master key. Here is the
example for ``keyring_file`` plugin or component:

.. code-block:: bash

   $ xtrabackup --copy-back --target-dir=/data/backup --datadir=/data/mysql \
   --transition-key=MySecetKey --generate-new-master-key \
   --keyring-file-data=/var/lib/mysql-keyring/keyring

In case of ``keyring_vault``, it will look like this:

.. code-block:: bash

   $ xtrabackup --copy-back --target-dir=/data/backup --datadir=/data/mysql \
   --transition-key=MySecetKey --generate-new-master-key \
   --keyring-vault-config=/etc/vault.cnf

*xtrabackup* will generate a new master key, store it in the target keyring
vault server and re-encrypt the tablespace keys using this key.

.. rubric:: Making the Backup with a Stored Transition Key

Finally, there is an option to store a transition key in the keyring. In this case,
*xtrabackup* will need to access the same keyring file or vault server during
prepare and copy-back but does not depend on whether the server keys have been
purged.

In this scenario, the three stages of the backup process look as follows. 

.. doc-attribute warning is version specific; problem may be solved in a later release
   (review
   after v8.0.7
   jira-issue pxb-1904)

- Backup

  .. code-block:: bash

     $ xtrabackup --backup --user=root -p --target-dir=/data/backup \
     --generate-transition-key

- Prepare

  - ``keyring_file`` variant:

    .. code-block:: bash

       $ xtrabackup --prepare --target-dir=/data/backup \
       --keyring-file-data=/var/lib/mysql-keyring/keyring

  - ``keyring_vault`` variant:

    .. code-block:: bash

       $ xtrabackup --prepare --target-dir=/data/backup \
       --keyring-vault-config=/etc/vault.cnf

- Copy-back

  - ``keyring_file`` variant:

    .. code-block:: bash

       $ xtrabackup --copy-back --target-dir=/data/backup --datadir=/data/mysql \
       --generate-new-master-key --keyring-file-data=/var/lib/mysql-keyring/keyring

  - ``keyring_vault`` variant:

    .. code-block:: bash

       $ xtrabackup --copy-back --target-dir=/data/backup --datadir=/data/mysql \
       --generate-new-master-key --keyring-vault-config=/etc/vault.cnf
