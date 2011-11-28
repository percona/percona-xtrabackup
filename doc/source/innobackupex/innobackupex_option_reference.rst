=============================================
 The :program:`innobackupex` Option Reference
=============================================

.. program:: innobackupex

This page documents all of the command-line options for the :program:`innobackupex` Perl script.


Options
=======

.. option:: --help

   This option displays a help screen and exits.

.. option:: --version

   This option displays the |innobackupex| version and copyright notice and then exits.

.. option:: --apply-log

   Prepare a backup in ``BACKUP-DIR`` by applying the transaction log file named :file:`xtrabackup_logfile` located in the same directory. Also, create new transaction logs. The InnoDB configuration is read from the file :file:`backup-my.cnf` created by |innobackupex| when the backup was made.

.. option:: --redo-only

   This option is passed directly to xtrabackup's :option:`xtrabackup --apply-log-only` option. This forces :program:`xtrabackup` to skip the "rollback" phase and do a "redo" only. This is necessary if the backup will have incremental changes applied to it later. See the |xtrabackup| :doc:`documentation <../xtrabackup_bin/incremental_backups>` for details.

.. option:: --copy-back

    Copy all the files in a previously made backup from the backup directory to their original locations.

.. option:: --include

   This option is a regular expression to be matched against table names in ``databasename.tablename`` format. It is passed directly to :program:`xtrabackup` 's :option:`xtrabackup --tables` option. See the :program:`xtrabackup` documentation for details.

.. option:: --defaults-file

   This option accepts a string argument that specifies what file to read the default MySQL options from. It is also passed directly to :program:`xtrabackup` 's defaults-file option. See the :program:`xtrabackup` :doc:`documentation <../xtrabackup_bin/xtrabackup_binary>` for details.

.. option:: --databases

   This option accepts a string argument that specifies the list of databases that |innobackupex| should back up. The list is of the form ``databasename1[.table_name1] databasename2[.table_name2] ...``. If this option is not specified, all databases containing MyISAM and InnoDB tables will be backed up. Please make sure that :option:`--databases` contains all of the InnoDB databases and tables, so that all of the innodb.frm files are also backed up. In case the list is very long, this can be specified in a file, and the full path of the file can be specified instead of the list. (See option :option:`--tables-file`.)

.. option:: --tables-file

   This option accepts a string argument that specifies the file in which there are a list of names of the form ``database.table``, one per line. The option is passed directly to :program:`xtrabackup` 's :option:`--tables-file` option.

.. option:: --throttle

   This option accepts an integer argument that specifies the number of I/O operations (i.e., pairs of read+write) per second. It is passed directly to xtrabackup's :option:`xtrabackup --throttle` option.

.. option:: --export

   This option is passed directly to :option:`xtrabackup --export` option. It enables exporting individual tables for import into another server. See the |xtrabackup| documentation for details.

.. option:: --use-memory

   This option accepts a string argument that specifies the amount of memory in bytes for :program:`xtrabackup` to use for crash recovery while preparing a backup. Multiples are supported providing the unit (e.g. 1MB, 1GB). It is used only with the option :option:`--apply-log`. It is passed directly to |xtrabackup| 's :option:`xtrabackup --use-memory` option. See the |xtrabackup| documentation for details.

.. option:: --password = 'PASSWORD'

   This option accepts a string argument specifying the password to use when connecting to the database. It is passed to the :command:`mysql` child process without alteration. See :command:`mysql --help` for details.

.. option:: --user = 'USER'

   This option accepts a string argument that specifies the user (i.e., the *MySQL* username used when connecting to the server) to login as, if that's not the current user. It is passed to the mysql child process without alteration. See :command:`mysql --help` for details.

.. option:: --port

   This option accepts a string argument that specifies the port to use when connecting to the database server with TCP/IP. It is passed to the :command:`mysql` child process. It is passed to the :command:`mysql` child process without alteration. See :command:`mysql --help` for details.

.. option:: --socket

   This option accepts a string argument that specifies the socket to use when connecting to the local database server with a UNIX domain socket. It is passed to the mysql child process without alteration. See :command:`mysql --help` for details.

.. option:: --host

   This option accepts a string argument that specifies the host to use when connecting to the database server with TCP/IP. It is passed to the mysql child process without alteration. See :command:`mysql --help` for details.

.. option:: --no-timestamp

   This option prevents creation of a time-stamped subdirectory of the ``BACKUP-ROOT-DIR`` given on the command line. When it is specified, the backup is done in ``BACKUP-ROOT-DIR`` instead.

.. option:: --slave-info

   This option is useful when backing up a replication slave server. It prints the binary log position and name of the master server. It also writes this information to the :file:`xtrabackup_slave_info` file as a ``CHANGE MASTER`` command. A new slave for this master can be set up by starting a slave server on this backup and issuing a ``CHANGE MASTER`` command with the binary log position saved in the :file:`xtrabackup_slave_info` file.

.. option:: --no-lock

   Use this option to disable table lock with ``FLUSH TABLES WITH READ LOCK``. Use it only if ALL your tables are InnoDB and you **DO NOT CARE** about the binary log position of the backup.

.. option:: --ibbackup = 'autodetect'

   This option accepts a string argument that specifies which xtrabackup binary should be used. The string should be the command used to run *XtraBackup*. The option can be useful if the :program:`xtrabackup` binary is not in your search path or working directory and the database server is not accessible at the moment. If this option is not specified, :program:`innobackupex` attempts to determine the binary to use automatically. By default, :program:`xtrabackup` is the command used. When option :option:`--apply-log` is specified, the binary is used whose name is in the file :file:`xtrabackup_binary` in the backup directory, if that file exists, or will attempt to autodetect it. However, if :option:`--copy-back` is selected, :program:`xtrabackup` is used unless other is specified.

.. option:: --incremental

   This option tells :program:`xtrabackup` to create an incremental backup, rather than a full one. It is passed to the :program:`xtrabackup` child process. When this option is specified, either :option:`--incremental-lsn` or :option:`--incremental-basedir` can also be given. If neither option is given, option :option:`--incremental-basedir` is passed to :program:`xtrabackup` by default, set to the first timestamped backup directory in the backup base directory.

.. option:: --incremental-basedir

   This option accepts a string argument that specifies the directory containing the full backup that is the base dataset for the incremental backup. It is used with the :option:`--incremental` option.

.. option:: --incremental-dir

   This option accepts a string argument that specifies the directory where the incremental backup will be combined with the full backup to make a new full backup. It is used with the :option:`--incremental` option.

.. option:: --incremental-lsn

   This option accepts a string argument that specifies the log sequence number (:term:`LSN`) to use for the incremental backup. It is used with the :option:`--incremental` option. It is used instead of specifying :option:`--incremental-basedir`. For databases created by *MySQL* and *Percona Server* 5.0-series versions, specify the as two 32-bit integers in high:low format. For databases created in 5.1 and later, specify the LSN as a single 64-bit integer.

.. option:: --extra-lsndir

   This option accepts a string argument that specifies the directory in which to save an extra copy of the :file:`xtrabackup_checkpoints` file. It is passed directly to |xtrabackup| 's :option:`--extra-lsndir` option. See the :program:`xtrabackup` documentation for details.

.. option:: --remote-host

   This option accepts a string argument that specifies the remote host on which the backup files will be created, by using an ssh connection.

.. option:: --stream

   This option accepts a string argument that specifies the format in which to do the streamed backup. The backup will be done to ``STDOUT`` in the specified format. Currently, the only supported format is :command:`tar`. Uses :doc:`tar4ibd <../tar4ibd/tar4ibd_binary>`, which is available in *XtraBackup* distributions. If you specify a path after this option, it will be interpreted as the value of :option:`tmpdir`.

.. option:: --tmpdir

   This option accepts a string argument that specifies the location where a temporary file will be stored. It should be used when :option:`--remote-host` or :option:`--stream` is specified. For these options, the transaction log will first be stored to a temporary file, before streaming or copying to a remote host. This option specifies the location where that temporary file will be stored. If the option is not specifed, the default is to use the value of ``tmpdir`` read from the server configuration.

.. option:: --tar4ibd

   Uses :program:`tar4ibd` in :option:`--stream` mode. It is enabled by default if no other command is specified (e.g. :option:`--force-tar`). 

.. option:: --force-tar

   This option forces the use of :command:`tar` when creating a streamed backup, rather than :program:`tar4ibd`, which is the default.

.. option:: --scp-opt = '-Cp -c arcfour'

   This option accepts a string argument that specifies the command line options to pass to :command:`scp` when the option :option:`--remost-host` is specified. If the option is not specified, the default options are ``-Cp -c arcfour``.

.. option:: --parallel

   This option accepts an integer argument that specifies the number of threads the :program:`xtrabackup` child process should use to back up files concurrently.  Note that this option works on file level, that is, if you have several .ibd files, they will be copied in parallel. If you have just single big .ibd file, it will have no effect. It is passed directly to xtrabackup's :option:`xtrabackup --parallel` option. See the :program:`xtrabackup` documentation for details.

.. option:: --safe-slave-backup

   Stop slave SQL thread and wait to start backup until ``Slave_open_temp_tables`` in ``SHOW STATUS`` is zero. If there are no open temporary tables, the backup will take place, otherwise the SQL thread will be started and stopped until there are no open temporary tables. The backup will fail if ``Slave_open_temp_tables`` does not become zero after :option:`--safe-slave-backup-timeout` seconds. The slave SQL thread will be restarted when the backup finishes.

.. option:: --safe-slave-backup-timeout

   How many seconds :option:`--safe-slave-backup`` should wait for ``Slave_open_temp_tables`` to become zero. Defaults to 300 seconds.

.. option:: --rsync

   Use the :program:`rsync` utility to optimize local file
   transfers. When this option is specified, :program:`innobackupex`
   uses :program:`rsync` to copy all non-InnoDB files instead of
   spawning a separate :program:`cp` for each file, which can be much
   faster for servers with a large number of databases or tables.  This
   option cannot be used together with :option:`--remote-host` or
   :option:`--stream`.
