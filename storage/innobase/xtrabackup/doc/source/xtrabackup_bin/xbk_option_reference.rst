.. _xbk_option_reference:

============================================
 The :program:`xtrabackup` Option Reference
============================================

.. program:: xtrabackup

This page documents all of the command-line options for the :program:`xtrabackup` binary.

Options
=======

.. option:: --apply-log-only

   This option causes only the redo stage to be performed when preparing a backup. It is very important for incremental backups.

.. option:: --backup

   Make a backup and place it in :option:`--target-dir`. See :doc:`Creating a backup <creating_a_backup>`.

.. option:: --binlog-info

   This option controls how |Percona XtraBackup| should retrieve server's binary log coordinates corresponding to the backup. Possible values are ``OFF``, ``ON``, ``LOCKLESS`` and ``AUTO``. See the |Percona XtraBackup| :ref:`lockless_bin-log`  manual page for more information

.. option:: --close-files

   Do not keep files opened. When xtrabackup opens tablespace it normally doesn't close its file handle in order to handle the DDL operations correctly. However, if the number of tablespaces is really huge and can not fit into any limit, there is an option to close file handles once they are no longer accessed. |Percona XtraBackup| can produce inconsistent backups with this option enabled. Use at your own risk.

.. option::  --compact     

   Create a compact backup by skipping secondary index pages.

.. option:: --compress 

   This option tells |xtrabackup| to compress all output data, including the transaction log file and meta data files, using the specified compression algorithm. The only currently supported algorithm is 'quicklz'. The resulting files have the qpress archive format, i.e. every `*.qp` file produced by xtrabackup is essentially a one-file qpress archive and can be extracted and uncompressed by the `qpress <http://www.quicklz.com/>`_  file archiver.

.. option:: --compress-chunk-size=#

   Size of working buffer(s) for compression threads in bytes. The default value is 64K.

.. option:: --compress-threads=# 

   This option specifies the number of worker threads used by |xtrabackup| for parallel data compression. This option defaults to 1. Parallel compression ('--compress-threads') can be used together with parallel file copying ('--parallel'). For example, '--parallel=4 --compress --compress-threads=2' will create 4 IO threads that will read the data and pipe it to 2 compression threads. 

.. option:: --copy-back         

   Copy all the files in a previously made backup from the backup directory to their original locations.

.. option:: --create-ib-logfile

   This option is not currently implemented. To create the InnoDB log files, you must prepare the backup twice at present.

.. option:: --datadir=DIRECTORY

   The source directory for the backup. This should be the same as the datadir for your MySQL server, so it should be read from :file:`my.cnf` if that exists; otherwise you must specify it on the command line.

.. option:: --defaults-extra-file=[MY.CNF]

   Read this file after the global files are read. Must be given as the first option on the command-line.

.. option:: --defaults-file=[MY.CNF]

   Only read default options from the given file. Must be given as the first option on the command-line. Must be a real file; it cannot be a symbolic link.

.. option:: --defaults-group=GROUP-NAME

   This option is to set the group which should be read from the configuration file. This is used by innobackupex if you use the `--defaults-group` option. It is needed for mysqld_multi deployments.

.. option:: --export

   Create files necessary for exporting tables. See :doc:`Restoring Individual Tables <restoring_individual_tables>`.

.. option:: --extra-lsndir=DIRECTORY

   (for --backup): save an extra copy of the xtrabackup_checkpoints file in this directory.

.. option:: --incremental-basedir=DIRECTORY

   When creating an incremental backup, this is the directory containing the full backup that is the base dataset for the incremental backups.

.. option:: --incremental-dir=DIRECTORY

   When preparing an incremental backup, this is the directory where the incremental backup is combined with the full backup to make a new full backup.

.. option:: --incremental-force-scan

   When creating an incremental backup, force a full scan of the data pages in the instance being backuped even if the complete changed page bitmap data is available.

.. option:: --incremental-lsn=LSN

   When creating an incremental backup, you can specify the log sequence number (:term:`LSN`) instead of specifying :option:`--incremental-basedir`. For databases created by *MySQL* and *Percona Server* 5.0-series versions, specify the :term:`LSN` as two 32-bit integers in high:low format. For databases created in 5.1 and later, specify the :term:`LSN` as a single 64-bit integer.  ##ATTENTION##: If a wrong LSN value is specified (a user error which XtraBackup is unable to detect), the backup will be unusable. Be careful!

.. option:: --innodb-log-arch-dir=DIRECTORY

   This option is used to specify the directory containing the archived logs. It can only be used with the :option:`xtrabackup --prepare` option.

.. option:: --innodb-miscellaneous

   There is a large group of InnoDB options that are normally read from the :term:`my.cnf` configuration file, so that xtrabackup boots up its embedded InnoDB in the same configuration as your current server. You normally do not need to specify these explicitly. These options have the same behavior that they have in InnoDB or XtraDB. They are as follows: ::

    --innodb-adaptive-hash-index
    --innodb-additional-mem-pool-size
    --innodb-autoextend-increment
    --innodb-buffer-pool-size
    --innodb-checksums
    --innodb-data-file-path
    --innodb-data-home-dir
    --innodb-doublewrite-file
    --innodb-doublewrite
    --innodb-extra-undoslots
    --innodb-fast-checksum
    --innodb-file-io-threads
    --innodb-file-per-table
    --innodb-flush-log-at-trx-commit
    --innodb-flush-method
    --innodb-force-recovery
    --innodb-io-capacity
    --innodb-lock-wait-timeout
    --innodb-log-buffer-size
    --innodb-log-files-in-group
    --innodb-log-file-size
    --innodb-log-group-home-dir
    --innodb-max-dirty-pages-pct
    --innodb-open-files
    --innodb-page-size
    --innodb-read-io-threads
    --innodb-write-io-threads

.. option:: --keyring-file-data=FILENAME

   The path to the keyring file.

.. option:: --log-copy-interval=#

   This option specifies time interval between checks done by log copying thread in milliseconds (default is 1 second).

.. option:: --log-stream

   Makes xtrabackup not copy data files, and output the contents of the InnoDB log files to STDOUT until the :option:`--suspend-at-end` file is deleted. This option enables :option:`--suspend-at-end` automatically.

.. option:: --no-defaults

   Don't read default options from any option file. Must be given as the first option on the command-line.

.. option:: --databases=#

   This option specifies the list of databases and tables that should be backed up. The option accepts the list of the form ``"databasename1[.table_name1] databasename2[.table_name2] . . ."``.

.. option:: --databases-file=#

   This option specifies the path to the file containing the list of databases and tables that should be backed up. The file can contain the list elements of the form ``databasename1[.table_name1]``, one element per line.

.. option:: --parallel=#

   This option specifies the number of threads to use to copy multiple data files concurrently when creating a backup. The default value is 1 (i.e., no concurrent transfer).

.. option:: --prepare

   Makes :program:`xtrabackup` perform recovery on a backup created with :option:`--backup`, so that it is ready to use. See :doc:`preparing a backup <preparing_the_backup>`.

.. option:: --print-defaults

   Print the program argument list and exit. Must be given as the first option on the command-line.

.. option:: --print-param

   Makes :program:`xtrabackup` print out parameters that can be used for copying the data files back to their original locations to restore them. See :ref:`scripting-xtrabackup`.

.. option:: --rebuild_indexes

   Rebuild secondary indexes in InnoDB tables after applying the log. Only has effect with --prepare. 

.. option::  --rebuild_threads=# 

   Use this number of threads to rebuild indexes in a compact backup. Only has effect with --prepare and --rebuild-indexes.

.. option:: --reencrypt-for-server-id=<new_server_id>

   Use this option to start the server instance with different server_id from the one the encrypted backup was taken from, like a replication slave or a galera node. When this option is used, xtrabackup will, as a prepare step, generate a new master key with ID based on the new server_id, store it into keyring file and re-encrypt the tablespace keys inside of tablespace headers. Option should be passed for :option:`--prepare` (final step).

.. option:: --secure-auth       

   Refuse client connecting to server if it uses old (pre-4.1.1) protocol. (Enabled by default; use --skip-secure-auth to disable.)

.. option:: --server-id=#

   The server instance being backed up.

.. option:: --stats

   Causes :program:`xtrabackup` to scan the specified data files and print out index statistics.

.. option:: --stream=name 

   Stream all backup files to the standard output in the specified format. Currently supported formats are 'xbstream' and 'tar'.

.. option:: --suspend-at-end

   Causes :program:`xtrabackup` to create a file called :file:`xtrabackup_suspended` in the :option:`--target-dir`. Instead of exiting after copying data files, :program:`xtrabackup` continues to copy the log file, and waits until the :file:`xtrabackup_suspended` file is deleted. This enables xtrabackup and other programs to coordinate their work. See :ref:`scripting-xtrabackup`.

.. option:: --tables=name

   A regular expression against which the full tablename, in ``databasename.tablename`` format, is matched. If the name matches, the table is backed up. See :doc:`partial backups <partial_backups>`.

.. option:: --tables-file=name

   A file containing one table name per line, in databasename.tablename format. The backup will be limited to the specified tables. See :ref:`scripting-xtrabackup`.

.. option:: --target-dir=DIRECTORY

   This option specifies the destination directory for the backup. If the directory does not exist, :program:`xtrabackup` creates it. If the directory does exist and is empty, :program:`xtrabackup` will succeed. :program:`xtrabackup` will not overwrite existing files, however; it will fail with operating system error 17, ``file exists``.

   If this option is a relative path, it is interpreted as being relative to the current working directory from which :program:`xtrabackup` is executed.

.. option:: --throttle=#

   This option limits :option:`--backup` to the specified number of read+write pairs of operations per second. See :doc:`throttling a backup <throttling_backups>`.

.. option:: --tmpdir=name

   This option is currently not used for anything except printing out the correct tmpdir parameter when :option:`--print-param` is used.

.. option:: --to-archived-lsn=LSN

   This option is used to specify the LSN to which the logs should be applied when backups are being prepared. It can only be used with the :option:`xtrabackup --prepare` option.

.. option:: --use-memory=#

   This option affects how much memory is allocated for preparing a backup with :option:`--prepare`, or analyzing statistics with :option:`--stats`. Its purpose is similar to :term:`innodb_buffer_pool_size`. It does not do the same thing as the similarly named option in Oracle's InnoDB Hot Backup tool. The default value is 100MB, and if you have enough available memory, 1GB to 2GB is a good recommended value. Multiples are supported providing the unit (e.g. 1MB, 1M, 1GB, 1G).

.. option:: --version

   This option prints |xtrabackup| version and exits.
