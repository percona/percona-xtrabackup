.. _how_xtrabackup_works:

================================================================================
 How |Percona XtraBackup| Works
================================================================================

|Percona XtraBackup| is based on :term:`InnoDB`'s crash-recovery functionality.
It copies your |InnoDB| data files, which results in data that is internally
inconsistent; but then it performs crash recovery on the files to make them a
consistent, usable database again.

This works because |InnoDB| maintains a redo log, also called the transaction
log. This contains a record of every change to InnoDB data. When |InnoDB|
starts, it inspects the data files and the transaction log, and performs two
steps. It applies committed transaction log entries to the data files, and it
performs an undo operation on any transactions that modified data but did not
commit.

|Percona XtraBackup| works by remembering the log sequence number (:term:`LSN`)
when it starts, and then copying away the data files. It takes some time to do
this, so if the files are changing, then they reflect the state of the database
at different points in time. At the same time, |Percona XtraBackup| runs a
background process that watches the transaction log files, and copies changes
from it. |Percona XtraBackup| needs to do this continually because the
transaction logs are written in a round-robin fashion, and can be reused after
a while. |Percona XtraBackup| needs the transaction log records for every
change to the data files since it began execution.

|Percona XtraBackup| will use `Backup locks
<https://www.percona.com/doc/percona-server/5.6/management/backup_locks.html>`_
where available as a lightweight alternative to ``FLUSH TABLES WITH READ
LOCK``. This feature is available in |Percona Server| 5.6+. |Percona
XtraBackup| uses this automatically to copy non-InnoDB data to avoid blocking
DML queries that modify |InnoDB| tables. When backup locks are supported by the
server, |xtrabackup| will first copy |InnoDB| data, run the ``LOCK TABLES FOR
BACKUP`` and copy the |MyISAM| tables and :term:`.frm` files. Once this is
done, the backup of the files will begin. It will backup :term:`.frm`,
:term:`.MRG`, :term:`.MYD`, :term:`.MYI`, :term:`.TRG`, :term:`.TRN`,
:term:`.ARM`, :term:`.ARZ`, :term:`.CSM`, :term:`.CSV`, ``.par``, and
:term:`.opt` files.

.. note::

  Locking is done only for |MyISAM| and other non-InnoDB tables, and only
  **after** |Percona XtraBackup| is finished backing up all InnoDB/XtraDB data
  and logs. |Percona XtraBackup| will use `Backup locks
  <https://www.percona.com/doc/percona-server/5.6/management/backup_locks.html>`_
  where available as a lightweight alternative to ``FLUSH TABLES WITH READ
  LOCK``. This feature is available in |Percona Server| 5.6+. |Percona
  XtraBackup| uses this automatically to copy non-InnoDB data to avoid blocking
  DML queries that modify |InnoDB| tables.

After that |xtrabackup| will use ``LOCK BINLOG FOR BACKUP`` to block all
operations that might change either binary log position or
``Exec_Master_Log_Pos`` or ``Exec_Gtid_Set`` (i.e. master binary log
coordinates corresponding to the current SQL thread state on a replication
slave) as reported by ``SHOW MASTER/SLAVE STATUS``. |xtrabackup| will then
finish copying the REDO log files and fetch the binary log coordinates. After
this is completed |xtrabackup| will unlock the binary log and tables.

Finally, the binary log position will be printed to ``STDERR`` and |xtrabackup|
will exit returning 0 if all went OK.

Note that the ``STDERR`` of |xtrabackup| is not written in any file. You will
have to redirect it to a file, e.g., ``xtrabackup OPTIONS 2> backupout.log``.

It will also create the :ref:`following files <xtrabackup_files>` in the
directory of the backup.

During the prepare phase, |Percona XtraBackup| performs crash recovery against
the copied data files, using the copied transaction log file. After this is
done, the database is ready to restore and use.

The backed-up |MyISAM| and |InnoDB| tables will be eventually consistent with
each other, because after the prepare (recovery) process, |InnoDB|'s data is
rolled forward to the point at which the backup completed, not rolled back to
the point at which it started. This point in time matches where the ``FLUSH
TABLES WITH READ LOCK`` was taken, so the |MyISAM| data and the prepared
|InnoDB| data are in sync.

The |xtrabackup| and |innobackupex| tools both offer many features not
mentioned in the preceding explanation. Each tool's functionality is explained
in more detail further in the manual. In brief, though, the tools permit you to
do operations such as streaming and incremental backups with various
combinations of copying the data files, copying the log files, and applying the
logs to the data.

.. _copy-back-xbk:

Restoring a backup
------------------

To restore a backup with |xtrabackup| you can use the
:option:`xtrabackup --copy-back` or :option:`xtrabackup --move-back` options.

|xtrabackup| will read from the :file:`my.cnf` the variables :term:`datadir`,
:term:`innodb_data_home_dir`, :term:`innodb_data_file_path`,
:term:`innodb_log_group_home_dir` and check that the directories exist.

It will copy the |MyISAM| tables, indexes, etc. (:term:`.frm`, :term:`.MRG`,
:term:`.MYD`, :term:`.MYI`, :term:`.TRG`, :term:`.TRN`, :term:`.ARM`,
:term:`.ARZ`, :term:`.CSM`, :term:`.CSV`, ``par`` and :term:`.opt` files)
first, |InnoDB| tables and indexes next and the log files at last. It will
preserve file's attributes when copying them, you may have to change the files'
ownership to ``mysql`` before starting the database server, as they will be
owned by the user who created the backup.

Alternatively, the :option:`xtrabackup --move-back` option may be used to
restore a backup. This option is similar to :option:`xtrabackup --copy-back`
with the only difference that instead of copying files it moves them to their
target locations. As this option removes backup files, it must be used with
caution. It is useful in cases when there is not enough free disk space to hold
both data files and their backup copies.
