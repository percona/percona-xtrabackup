.. _known_issues:

==============================
 Known issues and limitations
==============================

There is a number of |Percona XtraBackup| related issues with compressed
|InnoDB| tables. These issues result from either server-side bugs, or OS
configuration and thus, cannot be fixed on the |Percona XtraBackup| side.

Known issues:

 * For |MySQL| or |Percona Server| versions 5.1 and 5.5 there are known and
   unfixed bugs with redo-logging of updates to compressed |InnoDB| tables. For
   example, internal Oracle bug #16267120 has been fixed only in |MySQL|
   5.6.12, but not in 5.1 or 5.5. The bug is about compressed page images not
   being logged on page reorganization and thus, creating a possibility for
   recovery process to fail in case a different zlib version is being used when
   replaying a ``MLOG_ZIP_PAGE_REORGANIZE`` redo log record.

 * For |MySQL| or |Percona Server| version 5.6 it is **NOT** recommended to set
   ``innodb_log_compressed_pages=OFF`` for servers that use compressed |InnoDB|
   tables which are backed up with |Percona XtraBackup|. This option makes
   |InnoDB| recovery (and thus, backup prepare) sensible to ``zlib`` versions.
   In case the host where a backup prepare is performed uses a different
   ``zlib`` version than the one that was used by the server during runtime,
   backup prepare may fail due to differences in compression algorithms.

 * Backed-up table data could not be recovered if backup was taken while
   running ``OPTIMIZE TABLE`` (bug :bug:`1541763`) or ``ALTER TABLE ...
   TABLESPACE`` (bug :bug:`1532878`) on that table.

 * Compact Backups currently don't work due to bug :bug:`1192834`.

 * Backup fails with ``Error 24: 'Too many open files'``. This usually happens
   when database being backed up contains large amount of files and |Percona
   XtraBackup| can't open all of them to create a successful backup. In order
   to avoid this error the operating system should be configured appropriately
   so that |Percona XtraBackup| can open all its files. On Linux, this can be
   done with the ``ulimit`` command for specific backup session or by editing
   the :file:`/etc/security/limits.conf` to change it globally (**NOTE**: the
   maximum possible value that can be set up is ``1048576`` which is a
   hard-coded constant in the Linux kernel).

.. _xtrabackup_limitations:

The ``xtrabackup`` binary has some limitations you should be aware of to ensure
that your backups go smoothly and are recoverable.

Limitations:

 * The Aria storage engine is part of |MariaDB| and has been integrated in it
   for many years and Aria table files backup support has been added to
   |innobackupex| in 2011. The issue is that the engine uses recovery log files
   and an :file:`aria_log_control` file that are not backed up by
   |xtrabackup|. As stated in the `documentation
   <https://mariadb.com/kb/en/aria-faq/#when-is-it-safe-to-remove-old-log-files>`_,
   starting |MariaDB| without the :file:`maria_log_control` file will mark all
   the Aria tables as corrupted with this error when doing a ``CHECK`` on the
   table: ``Table is from another system and must be zerofilled or repaired to
   be usable on this system``. This means that the Aria tables from an
   |xtrabackup| backup must be repaired before being usable (this could be
   quite long depending on the size of the table). Another option is ``aria_chk
   --zerofil table`` on all Aria tables present on the backup after the prepare
   phase.

 * If the ``xtrabackup_logfile`` is larger than 4GB, the ``--prepare`` step
   will fail on 32-bit versions of ``xtrabackup``.

 * ``xtrabackup`` doesn't understand the very old ``--set-variable`` ``my.cnf``
   syntax that MySQL uses. See :ref:`configuring`.
