=============================================
 The :program:`innobackupex` Option Reference
=============================================

.. program:: innobackupex

This page documents all of the command-line options for the :program:`innobackupex`. 


Options
=======

.. option:: --apply-log

   Prepare a backup in ``BACKUP-DIR`` by applying the transaction log file named
   :file:`xtrabackup_logfile` located in the same directory. Also, create new
   transaction logs. The InnoDB configuration is read from the file
   :file:`backup-my.cnf` created by |innobackupex| when the backup was
   made. innobackupex --apply-log uses InnoDB configuration from
   ``backup-my.cnf`` by default, or from --defaults-file, if specified. InnoDB
   configuration in this context means server variables that affect data format,
   i.e. ``innodb_page_size``, ``innodb_log_block_size``, etc. Location-related
   variables, like ``innodb_log_group_home_dir`` or `innodb_data_file_path`` are
   always ignored by --apply-log, so preparing a backup always works with data
   files from the backup directory, rather than any external ones.

.. option:: --backup-locks

   This option controls if backup locks should be used instead of ``FLUSH TABLES
   WITH READ LOCK`` on the backup stage. The option has no effect when backup
   locks are not supported by the server. This option is enabled by default,
   disable with :option:`--no-backup-locks`.

.. option:: --no-backup-locks

   Explicity disables the option :option:`--backup-locks` which is enabled by
   default.

.. option:: --close-files

   Do not keep files opened. This option is passed directly to xtrabackup. When
   xtrabackup opens tablespace it normally doesn't close its file handle in
   order to handle the DDL operations correctly. However, if the number of
   tablespaces is really huge and can not fit into any limit, there is an option
   to close file handles once they are no longer accessed. |Percona XtraBackup|
   can produce inconsistent backups with this option enabled. Use at your own
   risk.

.. option:: --compress

   This option instructs xtrabackup to compress backup copies of InnoDB data
   files. It is passed directly to the xtrabackup child process. See the
   :program:`xtrabackup` :doc:`documentation
   <../xtrabackup_bin/xtrabackup_binary>` for details.

.. option::  --compress-threads=#

   This option specifies the number of worker threads that will be used for
   parallel compression. It is passed directly to the xtrabackup child
   process. See the :program:`xtrabackup` :doc:`documentation
   <../xtrabackup_bin/xtrabackup_binary>` for details.

.. option:: --compress-chunk-size=#

   This option specifies the size of the internal working buffer for each
   compression thread, measured in bytes. It is passed directly to the
   xtrabackup child process. The default value is 64K. See the
   :program:`xtrabackup` :doc:`documentation
   <../xtrabackup_bin/xtrabackup_binary>` for details.

.. option:: --copy-back

    Copy all the files in a previously made backup from the backup directory to
    their original locations. |Percona XtraBackup| :option:`innobackupex --copy-back`
    option will not copy over existing files unless
    :option:`innobackupex --force-non-empty-directories` option is specified.

.. option:: --databases=LIST

   This option specifies the list of databases that |innobackupex| should back
   up. The option accepts a string argument or path to file that contains the
   list of databases to back up. The list is of the form
   "databasename1[.table_name1] databasename2[.table_name2] . . .". If this
   option is not specified, all databases containing |MyISAM| and |InnoDB|
   tables will be backed up. Please make sure that --databases contains all of
   the |InnoDB| databases and tables, so that all of the innodb.frm files are
   also backed up. In case the list is very long, this can be specified in a
   file, and the full path of the file can be specified instead of the
   list. (See option --tables-file.)

.. option:: --decompress

   Decompresses all files with the .qp extension in a backup previously made
   with the :option:`innobackupex --compress` option. The :option:`innobackupex
   --parallel` option will allow multiple files to be decrypted and/or
   decompressed simultaneously. In order to decompress, the qpress utility MUST
   be installed and accessible within the path. |Percona XtraBackup| doesn't
   automatically remove the compressed files. In order to clean up the backup
   directory users should remove the ``*.qp`` files manually.

.. option:: --decrypt=ENCRYPTION-ALGORITHM

   Decrypts all files with the .xbcrypt extension in a backup previously made
   with --encrypt option. The :option:`innobackupex --parallel` option will
   allow multiple files to be decrypted and/or decompressed simultaneously.

.. option:: --defaults-file=[MY.CNF]

   This option accepts a string argument that specifies what file to read the
   default MySQL options from. Must be given as the first option on the
   command-line.

.. option:: --defaults-extra-file=[MY.CNF]

   This option specifies what extra file to read the default |MySQL| options
   from before the standard defaults-file. Must be given as the first option on
   the command-line.

.. option:: --defaults-group=GROUP-NAME

   This option accepts a string argument that specifies the group which should
   be read from the configuration file. This is needed if you use
   mysqld_multi. This can also be used to indicate groups other than mysqld and
   xtrabackup.

.. option:: --encrypt=ENCRYPTION_ALGORITHM

   This option instructs xtrabackup to encrypt backup copies of InnoDB data
   files using the algorithm specified in the ENCRYPTION_ALGORITHM. It is passed
   directly to the xtrabackup child process. See the :program:`xtrabackup`
   :doc:`documentation <../xtrabackup_bin/xtrabackup_binary>` for more details.

   Currently, the following algorithms are supported: ``AES128``,
   ``AES192`` and ``AES256``.

.. option:: --encrypt-key=ENCRYPTION_KEY

   This option instructs xtrabackup to use the given proper length encryption
   key as the ENCRYPTION_KEY when using the --encrypt option. It is passed
   directly to the xtrabackup child process. See the :program:`xtrabackup`
   :doc:`documentation <../xtrabackup_bin/xtrabackup_binary>` for more details.

   It is not recommended to use this option where there is uncontrolled access
   to the machine as the command line and thus the key can be viewed as part of
   the process info.

.. option:: --encrypt-key-file=ENCRYPTION_KEY_FILE

   This option instructs xtrabackup to use the encryption key stored in the
   given ENCRYPTION_KEY_FILE when using the --encrypt option. It is passed
   directly to the xtrabackup child process. See the :program:`xtrabackup`
   :doc:`documentation <../xtrabackup_bin/xtrabackup_binary>` for more details.

   The file must be a simple binary (or text) file that contains exactly the key
   to be used.

.. option:: --encrypt-threads=#

   This option specifies the number of worker threads that will be used for
   parallel encryption. It is passed directly to the xtrabackup child
   process. See the :program:`xtrabackup` :doc:`documentation
   <../xtrabackup_bin/xtrabackup_binary>` for more details.

.. option:: --encrypt-chunk-size=#

   This option specifies the size of the internal working buffer for each
   encryption thread, measured in bytes. It is passed directly to the xtrabackup
   child process. See the :program:`xtrabackup` :doc:`documentation
   <../xtrabackup_bin/xtrabackup_binary>` for more details.

.. option:: --export

   This option is passed directly to :option:`xtrabackup --export` option. It
   enables exporting individual tables for import into another server. See the
   |xtrabackup| documentation for details.

.. option:: --extra-lsndir=DIRECTORY

   This option accepts a string argument that specifies the directory in which
   to save an extra copy of the :file:`xtrabackup_checkpoints` file. It is
   passed directly to |xtrabackup|'s :option:`innobackupex --extra-lsndir` option. See the
   :program:`xtrabackup` documentation for details.

.. option:: --force-non-empty-directories 

   When specified, it makes :option:`innobackupex --copy-back` option or
   :option:`innobackupex --move-back` option transfer files to non-empty
   directories. No existing files will be overwritten. If --copy-back
   or --move-back has to copy a file from the backup directory which already
   exists in the destination directory, it will still fail with an error.

.. option:: --galera-info

   This options creates the ``xtrabackup_galera_info`` file which contains the
   local node state at the time of the backup. Option should be used when
   performing the backup of Percona-XtraDB-Cluster. Has no effect when backup
   locks are used to create the backup.

.. option:: --help

   This option displays a help screen and exits.

.. option:: --history=NAME

   This option enables the tracking of backup history in the
   ``PERCONA_SCHEMA.xtrabackup_history`` table. An optional history series name
   may be specified that will be placed with the history record for the current
   backup being taken.

.. option:: --host=HOST

   This option accepts a string argument that specifies the host to use when
   connecting to the database server with TCP/IP. It is passed to the mysql
   child process without alteration. See :command:`mysql --help` for details.

.. option:: --ibbackup=IBBACKUP-BINARY

   This option specifies which |xtrabackup| binary should be used. The option
   accepts a string argument. IBBACKUP-BINARY should be the command used to run
   |Percona XtraBackup|. The option can be useful if the |xtrabackup| binary is
   not in your search path or working directory. If this option is not
   specified, |innobackupex| attempts to determine the binary to use
   automatically.

.. option:: --include=REGEXP

   This option is a regular expression to be matched against table names in
   ``databasename.tablename`` format. It is passed directly to xtrabackup's
   :option:`xtrabackup --tables` option. See the :program:`xtrabackup`
   documentation for details.

.. option:: --incremental

   This option tells |xtrabackup| to create an incremental backup, rather than a
   full one. It is passed to the |xtrabackup| child process. When this option is
   specified, either :option:`innobackupex --incremental-lsn` or
   :option:`innobackupex --incremental-basedir` can also be given. If neither option is
   given, option :option:`innobackupex --incremental-basedir` is passed to
   :program:`xtrabackup` by default, set to the first timestamped backup
   directory in the backup base directory.

.. option:: --incremental-basedir=DIRECTORY

   This option accepts a string argument that specifies the directory containing
   the full backup that is the base dataset for the incremental backup. It is
   used with the :option:`innobackupex --incremental` option.

.. option:: --incremental-dir=DIRECTORY

   This option accepts a string argument that specifies the directory where the
   incremental backup will be combined with the full backup to make a new full
   backup. It is used with the :option:`innobackupex --incremental` option.

.. option:: --incremental-history-name=NAME

   This option specifies the name of the backup series stored in the
   :ref:`PERCONA_SCHEMA.xtrabackup_history <xtrabackup_history>` history record
   to base an incremental backup on. Percona Xtrabackup will search the history
   table looking for the most recent (highest innodb_to_lsn), successful backup
   in the series and take the to_lsn value to use as the starting lsn for the
   incremental backup. This will be mutually exclusive with
   :option:`innobackupex --incremental-history-uuid`, :option:`innobackupex --incremental-basedir`
   and :option:`innobackupex --incremental-lsn`. If no
   valid lsn can be found (no series by that name, no successful backups by that
   name) xtrabackup will return with an error. It is used with the
   :option:`innobackupex --incremental` option.

.. option:: --incremental-history-uuid=UUID

   This option specifies the UUID of the specific history record stored in the
   :ref:`PERCONA_SCHEMA.xtrabackup_history <xtrabackup_history>` to base an
   incremental backup on. :option:`innobackupex
   --incremental-history-name`,:optionL`innobackupex --incremental-basedir` and
   :option:`innobackupex --incremental-lsn`. If no valid lsn can be found (no
   success record with that uuid) xtrabackup will return with an error. It is
   used with the :option:`innobackupex --incremental` option.

.. option:: --incremental-lsn=LSN

   This option accepts a string argument that specifies the log sequence number
   (:term:`LSN`) to use for the incremental backup. It is used with the
   :option:`innobackupex --incremental` option. It is used instead of specifying
   :option:`innobackupex --incremental-basedir`. For databases created by *MySQL* and
   *Percona Server* 5.0-series versions, specify the as two 32-bit integers in
   high:low format. For databases created in 5.1 and later, specify the LSN as a
   single 64-bit integer.

.. option:: --kill-long-queries-timeout=SECONDS

   This option specifies the number of seconds innobackupex waits between
   starting ``FLUSH TABLES WITH READ LOCK`` and killing those queries that block
   it. Default is 0 seconds, which means innobackupex will not attempt to kill
   any queries. In order to use this option xtrabackup user should have
   ``PROCESS`` and ``SUPER`` privileges. Where supported (Percona Server 5.6+)
   xtrabackup will automatically use `Backup Locks
   <https://www.percona.com/doc/percona-server/5.6/management/backup_locks.html#backup-locks>`_
   as a lightweight alternative to ``FLUSH TABLES WITH READ LOCK`` to copy
   non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.

.. option:: --kill-long-query-type=all|select

   This option specifies which types of queries should be killed to unblock the
   global lock. Default is "all".

.. option:: --ftwrl-wait-timeout=SECONDS

   This option specifies time in seconds that innobackupex should wait for
   queries that would block ``FLUSH TABLES WITH READ LOCK`` before running
   it. If there are still such queries when the timeout expires, innobackupex
   terminates with an error. Default is 0, in which case innobackupex does not
   wait for queries to complete and starts ``FLUSH TABLES WITH READ LOCK``
   immediately. Where supported (Percona Server 5.6+) xtrabackup will
   automatically use `Backup Locks
   <https://www.percona.com/doc/percona-server/5.6/management/backup_locks.html#backup-locks>`_
   as a lightweight alternative to ``FLUSH TABLES WITH READ LOCK`` to copy
   non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.

.. option:: --ftwrl-wait-threshold=SECONDS

   This option specifies the query run time threshold which is used by
   innobackupex to detect long-running queries with a non-zero value of
   :option:`innobackupex --ftwrl-wait-timeout`. ``FLUSH TABLES WITH READ LOCK``
   is not started until such long-running queries exist. This option has no
   effect if --ftwrl-wait-timeout is 0. Default value is 60 seconds. Where
   supported (Percona Server 5.6+) xtrabackup will automatically use `Backup
   Locks
   <https://www.percona.com/doc/percona-server/5.6/management/backup_locks.html#backup-locks>`_
   as a lightweight alternative to ``FLUSH TABLES WITH READ LOCK`` to copy
   non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.

.. option:: --ftwrl-wait-query-type=all|update

   This option specifies which types of queries are allowed to complete before
   innobackupex will issue the global lock. Default is all.

.. option:: --log-copy-interval=#

   This option specifies time interval between checks done by log copying thread
   in milliseconds.

.. option:: --move-back

    Move all the files in a previously made backup from the backup directory to
    their original locations. As this option removes backup files, it must be
    used with caution.

.. option:: --no-lock

   Use this option to disable table lock with ``FLUSH TABLES WITH READ
   LOCK``. Use it only if ALL your tables are InnoDB and you **DO NOT CARE**
   about the binary log position of the backup. This option shouldn't be used if
   there are any ``DDL`` statements being executed or if any updates are
   happening on non-InnoDB tables (this includes the system MyISAM tables in the
   *mysql* database), otherwise it could lead to an inconsistent backup. Where
   supported (Percona Server 5.6+) xtrabackup will automatically use `Backup
   Locks
   <https://www.percona.com/doc/percona-server/5.6/management/backup_locks.html#backup-locks>`_
   as a lightweight alternative to ``FLUSH TABLES WITH READ LOCK`` to copy
   non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.  If
   you are considering to use :option:`innobackupex --no-lock` because your backups are
   failing to acquire the lock, this could be because of incoming replication
   events preventing the lock from succeeding. Please try using
   :option:`innobackupex --safe-slave-backup` to momentarily stop the replication slave
   thread, this may help the backup to succeed and you then don't need to resort
   to using this option.  :file:`xtrabackup_binlog_info` is not created
   when --no-lock option is used (because ``SHOW MASTER STATUS`` may be
   inconsistent), but under certain conditions
   :file:`xtrabackup_binlog_pos_innodb` can be used instead to get consistent
   binlog coordinates as described in :ref:`working_with_binlogs`.

.. option:: --no-timestamp

   This option prevents creation of a time-stamped subdirectory of the
   ``BACKUP-ROOT-DIR`` given on the command line. When it is specified, the
   backup is done in ``BACKUP-ROOT-DIR`` instead.

.. include:: ../.res/contents/option.no-version-check.txt

.. option:: --parallel=NUMBER-OF-THREADS

   This option accepts an integer argument that specifies the number of threads
   the :program:`xtrabackup` child process should use to back up files
   concurrently.  Note that this option works on file level, that is, if you
   have several .ibd files, they will be copied in parallel. If your tables are
   stored together in a single tablespace file, it will have no effect. This
   option will allow multiple files to be decrypted and/or decompressed
   simultaneously. In order to decompress, the qpress utility MUST be installed
   and accessable within the path. This process will remove the original
   compressed/encrypted files and leave the results in the same location. It is
   passed directly to xtrabackup's :option:`xtrabackup --parallel` option. See
   the :program:`xtrabackup` documentation for details

.. option:: --password=PASSWORD

   This option accepts a string argument specifying the password to use when
   connecting to the database. It is passed to the :command:`mysql` child
   process without alteration. See :command:`mysql --help` for details.

.. option:: --port=PORT

   This option accepts a string argument that specifies the port to use when
   connecting to the database server with TCP/IP. It is passed to the
   :command:`mysql` child process. It is passed to the :command:`mysql` child
   process without alteration. See :command:`mysql --help` for details.

.. option:: --rebuild-indexes

   This option only has effect when used together with the :option:`--apply-log <innobackupex --apply-log>`
   option and is passed directly to xtrabackup. When used, makes xtrabackup
   rebuild all secondary indexes after applying the log. This option is normally
   used to prepare compact backups. See the :program:`xtrabackup` documentation
   for more information.

.. option:: --rebuild-threads=NUMBER-OF-THREADS

   This option only has effect when used together with the :option:`innobackupex --apply-log`
   and :option:`innobackupex --rebuild-indexes` option and is passed directly to
   xtrabackup. When used, xtrabackup processes tablespaces in parallel with the
   specified number of threads when rebuilding indexes. See the
   :program:`xtrabackup` documentation for more information.

.. option:: --redo-only

   This option should be used when preparing the base full backup and when
   merging all incrementals except the last one. It is passed directly to
   xtrabackup's :option:`xtrabackup --apply-log-only` option. This forces
   :program:`xtrabackup` to skip the "rollback" phase and do a "redo" only. This
   is necessary if the backup will have incremental changes applied to it
   later. See the |xtrabackup| :doc:`documentation
   <../xtrabackup_bin/incremental_backups>` for details.

.. option:: --rsync

   Uses the :program:`rsync` utility to optimize local file transfers. When this
   option is specified, :program:`innobackupex` uses :program:`rsync` to copy
   all non-InnoDB files instead of spawning a separate :program:`cp` for each
   file, which can be much faster for servers with a large number of databases
   or tables.  This option cannot be used together with :option:`innobackupex --stream`.

.. option:: --safe-slave-backup

   When specified, innobackupex will stop the slave SQL thread just before
   running ``FLUSH TABLES WITH READ LOCK`` and wait to start backup until
   ``Slave_open_temp_tables`` in ``SHOW STATUS`` is zero. If there are no open
   temporary tables, the backup will take place, otherwise the SQL thread will
   be started and stopped until there are no open temporary tables. The backup
   will fail if ``Slave_open_temp_tables`` does not become zero after
   :option:`innobackupex --safe-slave-backup-timeout` seconds. The slave SQL
   thread will be restarted when the backup finishes.

.. option:: --safe-slave-backup-timeout=SECONDS

   How many seconds :option:`innobackupex --safe-slave-backup` should wait for
   ``Slave_open_temp_tables`` to become zero. Defaults to 300 seconds.

.. option:: --slave-info

   This option is useful when backing up a replication slave server. It prints
   the binary log position and name of the master server. It also writes this
   information to the :file:`xtrabackup_slave_info` file as a ``CHANGE MASTER``
   command. A new slave for this master can be set up by starting a slave server
   on this backup and issuing a ``CHANGE MASTER`` command with the binary log
   position saved in the :file:`xtrabackup_slave_info` file.

.. option:: --socket

   This option accepts a string argument that specifies the socket to use when
   connecting to the local database server with a UNIX domain socket. It is
   passed to the mysql child process without alteration. See :command:`mysql
   --help` for details.

.. option:: --stream=STREAMNAME

   This option accepts a string argument that specifies the format in which to
   do the streamed backup. The backup will be done to ``STDOUT`` in the
   specified format. Currently, supported formats are `tar` and `xbstream`. Uses
   :doc:`xbstream <../xbstream/xbstream>`, which is available in |Percona XtraBackup|
   distributions. If you specify a path after this option, it will be interpreted
   as the value of ``tmpdir``

.. option:: --tables-file=FILE

   This option accepts a string argument that specifies the file in which there
   are a list of names of the form ``database.table``, one per line. The option
   is passed directly to :program:`xtrabackup` 's :option:`innobackupex --tables-file`
   option.

.. option:: --throttle=#

   This option limits the number of chunks copied per second. The chunk size is
   *10 MB*. To limit the bandwidth to *10 MB/s*, set the option to *1*:
   `--throttle=1`.

   .. seealso::

      More information about how to throttle a backup
         :ref:`throttling_backups`

.. option:: --tmpdir=DIRECTORY

   This option accepts a string argument that specifies the location where a
   temporary file will be stored. It may be used when :option:`innobackupex --stream` is
   specified. For these options, the transaction log will first be stored to a
   temporary file, before streaming or copying to a remote host. This option
   specifies the location where that temporary file will be stored. If the
   option is not specified, the default is to use the value of ``tmpdir`` read
   from the server configuration. innobackupex is passing the tmpdir value
   specified in my.cnf as the --target-dir option to the xtrabackup binary. Both
   [mysqld] and [xtrabackup] groups are read from my.cnf. If there is tmpdir in
   both, then the value being used depends on the order of those group in
   my.cnf.

.. option:: --use-memory=#

   This option accepts a string argument that specifies the amount of memory in
   bytes for :program:`xtrabackup` to use for crash recovery while preparing a
   backup. Multiples are supported providing the unit (e.g. 1MB, 1M, 1GB,
   1G). It is used only with the option :option:`innobackupex --apply-log`. It is passed
   directly to xtrabackup's :option:`xtrabackup --use-memory` option. See the
   |xtrabackup| documentation for details.

.. option:: --user=USER

   This option accepts a string argument that specifies the user (i.e., the
   *MySQL* username used when connecting to the server) to login as, if that's
   not the current user. It is passed to the mysql child process without
   alteration. See :command:`mysql --help` for details.

.. option:: --version

   This option displays the |innobackupex| version and copyright notice and then
   exits.

.. |program| replace:: :program:`innobackupex`
