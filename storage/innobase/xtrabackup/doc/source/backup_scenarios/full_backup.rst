.. _full_backup:

================================================================================
The Backup Cycle - Full Backups
================================================================================

.. contents::
   :local:

.. _creating_a_backup:

Creating a backup with |cmd.~backup|
================================================================================

To create a backup, run :program:`xtrabackup` with the :option:`--backup`
option. You also need to pass the |param.target-dir| parameter to specify where
the backup will be stored, if the |InnoDB| data or log files are not stored in
the same directory, you might need to specify the location of those, too. If the
target directory does not exist, |xtrabackup| creates it. If the target directory
exists and is empty, |cmd.~backup| will succeed.

.. warning::

   |xtrabackup| will not overwrite existing files, it will fail with operating
   system error 17: ``file exists``.

To start the backup process run:

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/data/backups/

When |cmd.~backup| completes, the created backup is available from the
path that you have provided as the value of the |param.target-dir|
(|file.data.backups|). A relative path is allowed.

During the backup process, you see a lot of output showing the progress of data
files being copied, as well as the log file thread repeatedly scanning the log
files and copying from it. Here is an example that shows the log thread
scanning the log in the background, and a file copying thread working on the
:file:`ibdata1` file:

.. code-block:: text

   160906 10:19:17 Finished backing up non-InnoDB tables and files
   160906 10:19:17 Executing FLUSH NO_WRITE_TO_BINLOG ENGINE LOGS...
   xtrabackup: The latest check point (for incremental): '62988944'
   xtrabackup: Stopping log copying thread.
   .160906 10:19:18 >> log scanned up to (137343534)
   160906 10:19:18 Executing UNLOCK TABLES
   160906 10:19:18 All tables unlocked
   160906 10:19:18 Backup created in directory '/data/backups/'
   160906 10:19:18 [00] Writing backup-my.cnf
   160906 10:19:18 [00]        ...done
   160906 10:19:18 [00] Writing xtrabackup_info
   160906 10:19:18 [00]        ...done
   xtrabackup: Transaction log of lsn (26970807) to (137343534) was copied.
   160906 10:19:18 completed OK!

The last thing you should see is something like the following, where the value
of the ``<LSN>`` will be a number that depends on your system:

.. code-block:: text

   $ xtrabackup: Transaction log of lsn (<SLN>) to (<LSN>) was copied.

.. note::

   Log copying thread checks the transactional log every second to see if there
   were any new log records written that need to be copied, but there is a
   chance that the log copying thread might not be able to keep up with the
   amount of writes that go to the transactional logs, and will hit an error
   when the log records are overwritten before they could be read.

After the backup is finished, the target directory will contain files such as
the following, assuming you have a single InnoDB table :file:`test.tbl1` and you
are using MySQL's :term:`innodb_file_per_table` option:

.. code-block:: bash

   $ ls -lh /data/backups/
   total 182M
   drwx------  7 root root 4.0K Sep  6 10:19 .
   drwxrwxrwt 11 root root 4.0K Sep  6 11:05 ..
   -rw-r-----  1 root root  387 Sep  6 10:19 backup-my.cnf
   -rw-r-----  1 root root  76M Sep  6 10:19 ibdata1
   drwx------  2 root root 4.0K Sep  6 10:19 mysql
   drwx------  2 root root 4.0K Sep  6 10:19 performance_schema
   drwx------  2 root root 4.0K Sep  6 10:19 sbtest
   drwx------  2 root root 4.0K Sep  6 10:19 test
   drwx------  2 root root 4.0K Sep  6 10:19 world2
   -rw-r-----  1 root root  116 Sep  6 10:19 xtrabackup_checkpoints
   -rw-r-----  1 root root  433 Sep  6 10:19 xtrabackup_info
   -rw-r-----  1 root root 106M Sep  6 10:19 xtrabackup_logfile

For larger databases, the backup can take a long time. You may cancel
the backup process at any time since |xtrabackup| does not modify 
the data on the source database.

The next step is getting your backup ready to be restored.

.. _preparing_a_backup:

Preparing a backup using |cmd.~prepare|
================================================================================

As data on the server could change while the backup was being made, data files
may be not point-in-time consistent after you run |cmd.~backup|.

If you try to start the server without the |cmd.~prepare| step, |innodb| will
detect the data corruption and stop working in order to avoid running on damaged
data. |cmd.~prepare| makes the files consistent, so that you can run |innodb| on
them.

.. important::

   |percona-xtrabackup| 8.0 can only *prepare* backups of |MySQL| 8.0,
   |percona-server| 8.0, and |percona-xtradb-cluster| 8.0 databases. Releases
   prior to 8.0 are not supported.

You may run |cmd.~prepare| either on the instance where the backup has been made
or the target server where you intend to restore the backup. You can copy the
backup to a utility server and prepare it there, too.

.. admonition:: Implementation detail: Imbedded InnoDB

   During the *prepare* operation, |xtrabackup| boots up a modified
   embedded InnoDB (the libraries |xtrabackup| was linked against). The
   modifications are necessary to disable InnoDB standard safety checks, such as
   complaining about the log file not being the right size. This warning is not
   appropriate for working with backups. These modifications are only for the
   xtrabackup binary; you do not need a modified |InnoDB| to use |xtrabackup| for
   your backups.

   The *prepare* step uses this "embedded InnoDB" to perform crash recovery on the
   copied data files, using the copied log file.

Running the *prepare* step is easy: you simply run |cmd.~prepare| passing the
directory that contains the backup to prepare as the value of the |param.target-dir| parameter:

.. code-block:: bash

   $ xtrabackup --prepare --target-dir=/data/backups/

When |cmd.~prepare| finishes, you should see an ``InnoDB shutdown`` with a message such
as the following, where again the value of :term:`LSN` will depend on your
system:

.. code-block:: text

  InnoDB: Shutdown completed; log sequence number 137345046
  160906 11:21:01 completed OK!

.. warning::

   The validity of the backup is not guaranteed and data corruption may incur if
   |cmd.~prepare| is interrupted.

Running |cmd.~prepare| again on the already prepared backup has no
effect. |xtrabackup| recognizes that backup is already prepared and informs you
so:

.. code-block:: console

   xtrabackup: This target seems to be already prepared.
   xtrabackup: notice: xtrabackup_logfile was already used to '--prepare'.

.. tip::

   If you intend to use the backup as the basis for further incremental backups,
   you should use the :option:`--apply-log-only` option when preparing the
   backup, or you will not be able to apply incremental backups to it. See the
   documentation on preparing :ref:`incremental backups <incremental_backup>`
   for more details.

.. _restoring_a_backup:

Restoring a backup using |cmd.~copy-back|
================================================================================

.. warning::

  Backup needs to be :ref:`prepared <preparing_a_backup>` before it can be
  restored.

For convenience, |xtrabackup| binary has the :option:`--copy-back`
option to copy the backup to the :term:`datadir` of the server:

.. code-block:: bash

  $ xtrabackup --copy-back --target-dir=/data/backups/

If you don't want to save your backup, you can use the
:option:`--move-back` option which will move the backed up data to
the :term:`datadir`.

If you don't want to use any of the above options, you can additionally use
:program:`rsync` or :program:`cp` to restore the files.

.. note::

   The :term:`datadir` must be empty before restoring the backup. Also it's
   important to note that MySQL server needs to be shut down before restore is
   performed. You cannot restore to a :term:`datadir` of a running mysqld instance
   (except when importing a partial backup).

Example of the :program:`rsync` command that can be used to restore the backup
can look like this:

.. code-block:: bash

   $ rsync -avrP /data/backup/ /var/lib/mysql/

You should check that the restored files have the correct ownership and
permissions.

As files' attributes will be preserved, in most cases you will need to change
the files' ownership to ``mysql`` before starting the database server, as they
will be owned by the user who created the backup:

.. code-block:: bash

  $ chown -R mysql:mysql /var/lib/mysql

Data is now restored and you can start the server.

.. Replacements ================================================================

.. include:: ../_res/replace/command.txt
.. include:: ../_res/replace/file.txt
.. include:: ../_res/replace/parameter.txt
.. include:: ../_res/replace/proper.txt
