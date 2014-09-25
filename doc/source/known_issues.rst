.. _known_issues:

==============================
 Known issues and limitations
==============================

There is a number of |Percona XtraBackup| related issues with compressed |InnoDB| tables. These issues result from either server-side bugs, or OS configuration and thus, cannot be fixed on the |Percona XtraBackup| side.

Known issues:

 * For |MySQL| or |Percona Server| versions 5.1 and 5.5 there are known and unfixed bugs with redo-logging of updates to compressed |InnoDB| tables. For example, internal Oracle bug #16267120 has been fixed only in |MySQL| 5.6.12, but not in 5.1 or 5.5. The bug is about compressed page images not being logged on page reorganization and thus, creating a possibility for recovery process to fail in case a different zlib version is being used when replaying a ``MLOG_ZIP_PAGE_REORGANIZE`` redo log record.

 * For |MySQL| or |Percona Server| version 5.6 it is NOT recommended to set ``innodb_log_compressed_pages=OFF`` for servers that use compressed |InnoDB| tables which are backed up with |Percona XtraBackup|. This option makes |InnoDB| recovery (and thus, backup prepare) sensible to ``zlib`` versions. In case the host where a backup prepare is performed uses a different ``zlib`` version than the one that was used by the server during runtime, backup prepare may fail due to differences in compression algorithms.
