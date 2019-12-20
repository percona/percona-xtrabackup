.. _lockless_bin-log:

===============================
Lockless binary log information
===============================

When the `Lockless binary log information
<https://www.percona.com/doc/percona-server/5.6/management/backup_locks.html#backup-safe-binlog-information>`_
feature is available [#n-1]_ on the server, |Percona XtraBackup| can trust
binary log information stored in the |InnoDB| system header and avoid executing
``LOCK BINLOG FOR BACKUP`` (and thus, blocking commits for the duration of
finalizing the ``REDO`` log copy) under a number of circumstances:

* when the server is not a GTID-enabled Galera cluster node

* when the replication I/O thread information should not be stored as a part of
  the backup (i.e. when the :option:`xtrabackup --slave-info` option is not
  specified)

If all of the above conditions hold, |Percona XtraBackup| does not execute the
``SHOW MASTER STATUS`` as a part of the backup procedure, does not create the
:file:`xtrabackup_binlog_info` file on backup. Instead, that information is
retrieved and the file is created after preparing the backup, along with
creating :file:`xtrabackup_binlog_pos_innodb`, which in this case contains
exactly the same information as in :file:`xtrabackup_binlog_info` and is thus
redundant.

To make this new functionality configurable, there is now a new |Percona
XtraBackup| option, :option:`xtrabackup --binlog-info`, which can accept the
following values:

* ``OFF`` - This means that |Percona XtraBackup| will not attempt to retrieve
  the binary log information at all, neither during the backup creation, nor
  after preparing it. This can help when a user just wants to copy data without
  any meta information like binary log or replication coordinates. In this
  case, ``xtrabackup --binlog-info=OFF`` can be passed to |Percona
  XtraBackup| and ``LOCK BINLOG FOR BACKUP`` will not be executed, even if the
  backup-safe binlog info feature is not provided by the server (but the backup
  locks feature is still a requirement).

* ``ON`` - This matches the old behavior, i.e. the one before this |Percona
  XtraBackup| feature had been implemented. When specified, |Percona
  XtraBackup| retrieves the binary log information and uses ``LOCK BINLOG FOR
  BACKUP`` (if available) to ensure its consistency.

* ``LOCKLESS`` - This corresponds to the functionality explained above:
  |Percona XtraBackup| will not retrieve binary log information during the
  backup process, will not execute ``LOCK BINLOG FOR BACKUP``, and the
  :file:`xtrabackup_binlog_info` file will not be created. The file will be
  created after preparing the backup using the information stored in the InnoDB
  system header. If the required server-side functionality is not provided by
  the server, specifying this :option:`xtrabackup --binlog-info` value will
  result in an error. If one of the above mentioned conditions does not hold,
  ``LOCK BINLOG FOR BACKUP`` will still be executed to ensure consistency of
  other meta data.

* ``AUTO`` - This is the default value. When used, |Percona XtraBackup| will
  automatically switch to either ``ON`` or ``LOCKLESS``, depending on the
  server-side feature availability, i.e., whether the
  ``have_backup_safe_binlog_info`` server variable is available.

.. rubric:: Footnotes

.. [#n-1]

  This feature is exclusive to |Percona Server| starting with version
  5.6.26-74.0. It is also used in |Percona XtraDB Cluster| when the
  node is being backed up without :option:`xtrabackup --galera-info`.
