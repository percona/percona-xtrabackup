.. _how_ibk_works:

================================================================================
 How |innobackupex| Works
================================================================================

From |Percona XtraBackup| version 2.3 :program:`innobackupex` is has been
rewritten in *C* and set up as a symlink to the
:program:`xtrabackup`. |innobackupex| supports all features and syntax as 2.2
version did, but it is now deprecated and will be removed in next major
release. Syntax for new features will not be added to the innobackupex, only to
the xtrabackup.

The following describes the rationale behind :program:`innobackupex` actions.

.. _making-backup-ibk:

Making a Backup
================================================================================

If no mode is specified, |innobackupex| will assume the backup mode.

By default, it runs :program:`xtrabackup` and lets it copy the
InnoDB data files. When :program:`xtrabackup` finishes that,
:program:`innobackupex` sees it create the :file:`xtrabackup_suspended_2` file
and executes ``FLUSH TABLES WITH READ LOCK``. :program:`xtrabackup` will use
`Backup locks
<https://www.percona.com/doc/percona-server/5.7/management/backup_locks.html#backup-locks>`_
where available as a lightweight alternative to ``FLUSH TABLES WITH READ
LOCK``. This feature is available in |Percona Server| 5.6+. |Percona XtraBackup|
uses this automatically to copy non-InnoDB data to avoid blocking DML queries
that modify InnoDB tables. Then it begins copying the rest of the files.

|innobackupex| will then check |MySQL| variables to determine which features are
supported by server. Special interest are backup locks, changed page bitmaps,
GTID mode, etc. If everything goes well, the binary is started as a child
process.

|innobackupex| will wait for slaves in a replication setup if the option
:option:`innobackupex --safe-slave-backup` is set and will flush all tables with
**READ LOCK**, preventing all |MyISAM| tables from writing (unless option
:option:`innobackupex --no-lock` is specified).

.. note:: 

   Locking is done only for MyISAM and other non-InnoDB tables, and only
   **after** |Percona XtraBackup| is finished backing up all InnoDB/XtraDB data
   and logs. |Percona XtraBackup| will use ``backup locks``
   where available as a lightweight alternative to ``FLUSH TABLES WITH READ
   LOCK``. This feature is available in |Percona Server| 5.6+. |Percona
   XtraBackup| uses this automatically to copy non-InnoDB data to avoid blocking
   DML queries that modify InnoDB tables.

Once this is done, the backup of the files will begin. It will backup
:term:`.frm`, :term:`.MRG`, :term:`.MYD`, :term:`.MYI`, :term:`.TRG`,
:term:`.TRN`, :term:`.ARM`, :term:`.ARZ`, :term:`.CSM`, :term:`.CSV`, ``.par``,
and :term:`.opt` files.

When all the files are backed up, it resumes :program:`ibbackup` and wait until
it finishes copying the transactions done while the backup was done. Then, the
tables are unlocked, the slave is started (if the option :option:`innobackupex --safe-slave-backup`
was used) and the connection with the server is
closed. Then, it removes the :file:`xtrabackup_suspended_2` file and permits
:program:`xtrabackup` to exit.

It will also create the following files in the directory of the backup:

:file:`xtrabackup_checkpoints`
   containing the :term:`LSN` and the type of backup;

:file:`xtrabackup_binlog_info` 
   containing the position of the binary log at the moment of backing up;

:file:`xtrabackup_binlog_pos_innodb`
   containing the position of the binary log at the moment of backing up relative to |InnoDB| transactions;

:file:`xtrabackup_slave_info`
   containing the MySQL binlog position of the master server in a replication setup via ``SHOW SLAVE STATUS`` if the :option:`innobackupex --slave-info` option is passed;

:file:`backup-my.cnf`
   containing only the :file:`my.cnf` options required for the backup. For example, innodb_data_file_path, innodb_log_files_in_group, innodb_log_file_size, innodb_fast_checksum, innodb_page_size, innodb_log_block_size;

:file:`xtrabackup_binary` 
   containing the binary used for the backup;

:file:`mysql-stderr`
  containing the ``STDERR`` of :program:`mysqld` during the process and

:file:`mysql-stdout`
  containing the ``STDOUT`` of the server.

Finally, the binary log position will be printed to ``STDERR`` and |innobackupex| will exit returning 0 if all went OK.

Note that the ``STDERR`` of |innobackupex| is not written in any file. You will have to redirect it to a file, e.g., ``innobackupex OPTIONS 2> backupout.log``.

.. _copy-back-ibk:

Restoring a backup
==================

To restore a backup with |innobackupex| the :option:`innobackupex --copy-back` option must be used.

|innobackupex| will read from the :file:`my.cnf` the variables :term:`datadir`,
:term:`innodb_data_home_dir`, :term:`innodb_data_file_path`,
:term:`innodb_log_group_home_dir` and check that the directories exist.

It will copy the |MyISAM| tables, indexes, etc. (:term:`.frm`, :term:`.MRG`,
:term:`.MYD`, :term:`.MYI`, :term:`.TRG`, :term:`.TRN`, :term:`.ARM`,
:term:`.ARZ`, :term:`.CSM`, :term:`.CSV`, ``par`` and :term:`.opt` files) first,
|InnoDB| tables and indexes next and the log files at last. It will preserve
file's attributes when copying them, you may have to change the files' ownership
to ``mysql`` before starting the database server, as they will be owned by the
user who created the backup.

Alternatively, the :option:`innobackupex --move-back` option may be used to restore a
backup. This option is similar to :option:`innobackupex --copy-back` with the only
difference that instead of copying files it moves them to their target
locations. As this option removes backup files, it must be used with
caution. It is useful in cases when there is not enough free disk space
to hold both data files and their backup copies.
