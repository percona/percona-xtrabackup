.. _compressed_backup:

=================
Compressed Backup
=================

*Percona XtraBackup* supports compressed backups: a local or streaming backup
can be compressed or decompressed with *xbstream*.

Creating Compressed Backups
===========================

In order to make a compressed backup you'll need to use the :option:`--compress`
option:

.. code-block:: bash

  $ xtrabackup --backup --compress --target-dir=/data/compressed/

The :option:`--compress` uses the ``qpress`` tool that you can install via
the ``percona-release`` package configuration tool as follows:

.. code-block:: bash

   $ sudo percona-release enable tools
   $ sudo apt update
   $ sudo apt install qpress

.. note::

   .. include:: ../.res/contents/instruction.repository.enabling.txt

   .. seealso:: :ref:`installing_from_binaries`

If you want to speed up the compression you can use the parallel compression,
which can be enabled with :option:`--compress-threads` option.
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
*Percona XtraBackup* has implemented :option:`--decompress` option
that can be used to decompress the backup.


.. code-block:: bash

   $ xtrabackup --decompress --target-dir=/data/compressed/

.. note::

  :option:`--parallel` can be used with
  :option:`--decompress` option to decompress multiple files
  simultaneously.

*Percona XtraBackup* doesn't automatically remove the compressed files. In
order to clean up the backup directory you should use
:option:`--remove-original` option. Even if they're not removed
these files will not be copied/moved over to the datadir if
:option:`--copy-back` or :option:`--move-back` are used.

When the files are uncompressed you can prepare the backup with the
:option:`--prepare` option:

.. code-block:: bash

  $ xtrabackup --prepare --target-dir=/data/compressed/

You should check for a confirmation message: ::

  InnoDB: Starting shutdown...
  InnoDB: Shutdown completed; log sequence number 9293846
  170223 13:39:31 completed OK!

Now the files in :file:`/data/compressed/` are ready to be used by the server.

Restoring the backup
--------------------

*xtrabackup* has a :option:`--copy-back` option, which performs the
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
