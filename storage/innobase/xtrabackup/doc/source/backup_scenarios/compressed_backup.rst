.. _compressed_backup:

=================
Compressed Backup
=================

|Percona XtraBackup| has implemented support for compressed backups. It can be
used to compress/decompress local or streaming backup with |xbstream|.

Creating Compressed Backups
===========================

In order to make a compressed backup you'll need to use
:option:`xtrabackup --compress` option:

.. code-block:: bash

  $ xtrabackup --backup --compress --target-dir=/data/compressed/

If you want to speed up the compression you can use the parallel compression,
which can be enabled with :option:`xtrabackup --compress-threads` option.
Following example will use four threads for compression:

.. code-block:: bash

  $ xtrabackup --backup --compress --compress-threads=4 \
  --target-dir=/data/compressed/

Output should look like this ::

  ...
  170223 13:00:38 [01] Compressing ./test/sbtest1.frm to /tmp/compressed/test/sbtest1.frm.qp
  170223 13:00:38 [01]        ...done
  170223 13:00:38 [01] Compressing ./test/sbtest2.frm to /tmp/compressed/test/sbtest2.frm.qp
  170223 13:00:38 [01]        ...done
  ...
  170223 13:00:39 [00] Compressing xtrabackup_info
  170223 13:00:39 [00]        ...done
  xtrabackup: Transaction log of lsn (9291934) to (9291934) was copied.
  170223 13:00:39 completed OK!

Preparing the backup
--------------------

Before you can prepare the backup you'll need to uncompress all the files.
|Percona XtraBackup| has implemented :option:`xtrabackup --decompress` option
that can be used to decompress the backup.

.. note::

  Before proceeding you'll need to make sure that `qpress
  <http://www.quicklz.com/>`_ has been installed. It's availabe from
  :ref:`Percona Software repositories <installing_from_binaries>`


.. code-block:: bash

 $ xtrabackup --decompress --target-dir=/data/compressed/

.. note::

  :option:`xtrabackup --parallel` can be used with
  :option:`xtrabackup --decompress` option to decompress multiple files
  simultaneously.

|Percona XtraBackup| doesn't automatically remove the compressed files. In
order to clean up the backup directory you should use
:option:`xtrabackup --remove-original` option. Even if they're not removed
these files will not be copied/moved over to the datadir if
:option:`xtrabackup --copy-back` or :option:`xtrabackup --move-back` are used.

When the files are uncompressed you can prepare the backup with the
:option:`xtrabackup --prepare` option:

.. code-block:: bash

  $ xtrabackup --prepare --target-dir=/data/compressed/

You should check for a confirmation message: ::

  InnoDB: Starting shutdown...
  InnoDB: Shutdown completed; log sequence number 9293846
  170223 13:39:31 completed OK!

Now the files in :file:`/data/compressed/` are ready to be used by the server.

Restoring the backup
--------------------

|xtrabackup| has a :option:`xtrabackup --copy-back` option, which performs the
restoration of a backup to the server's :term:`datadir`:

.. code-block:: bash

  $ xtrabackup --copy-back --target-dir=/data/backups/

It will copy all the data-related files back to the server's :term:`datadir`,
determined by the server's :file:`my.cnf` configuration file. You should check
the last line of the output for a success message::

  170223 13:49:13 completed OK!

You should check the file permissions after copying the data back. You may need
to adjust them with something like:

.. code-block:: bash

  $ chown -R mysql:mysql /var/lib/mysql

Now that the :term:`datadir` contains the restored data. You are ready to start
the server.
