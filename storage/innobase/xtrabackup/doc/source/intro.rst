.. _intro:

==========================
 About Percona XtraBackup
==========================

*Percona XtraBackup* is the world's only open-source, free |MySQL| hot backup
software that performs non-blocking backups for |InnoDB| and |XtraDB|
databases. With *Percona XtraBackup*, you can achieve the following benefits:

* Backups that complete quickly and reliably

* Uninterrupted transaction processing during backups

* Savings on disk space and network bandwidth

* Automatic backup verification

* Higher uptime due to faster restore time

|Percona XtraBackup| makes |MySQL| hot backups for all versions of
|Percona Server|, |MySQL|, and |MariaDB|. It performs streaming,
compressed, and incremental |MySQL| backups.

.. include:: .res/contents/important.mariadb-support.txt

|Percona XtraBackup| works with |MySQL|, |MariaDB|, and |Percona
Server|. It supports completely non-blocking backups of
|InnoDB|, |XtraDB|, and *HailDB* storage engines. In addition, it can
back up the following storage engines by briefly pausing writes at the
end of the backup: |MyISAM|, :term:`Merge <.MRG>`, and :term:`Archive
<.ARM>`, including partitioned tables, triggers, and database options.

.. important::

   |Percona XtraBackup| 2.4 does not support the MyRocks or TokuDB storage engines.

Percona's enterprise-grade commercial `MySQL Support
<http://www.percona.com/mysql-support/>`_ contracts include support for
|Percona XtraBackup|. We recommend support for critical production deployments.

MySQL Backup Tool Feature Comparison
====================================

.. tabularcolumns:: |p{5cm}|p{5cm}|p{5cm}|

.. list-table::
   :header-rows: 1

   * - Features
     - Percona XtraBackup
     - MySQL Enterprise backup
   * - License
     - GPL
     - Proprietary
   * - Price
     - Free
     - Included in subscription at $5000 per Server
   * - Streaming and encryption formats
     - Open source
     - Proprietary
   * - Supported |MySQL| flavors
     - |MySQL|, |Percona Server|, |MariaDB|, |Percona XtraDB Cluster| [#n-9]_,
       *MariaDB Galera Cluster*
     - |MySQL|
   * - Supported operating systems
     - Linux
     - Linux, Solaris, Windows, OSX, FreeBSD.
   * - Non-blocking InnoDB backups [#n-1]_
     - Yes
     - Yes
   * - Blocking MyISAM backups
     - Yes
     - Yes
   * - Incremental backups
     - Yes
     - Yes
   * - Full compressed backups
     - Yes
     - Yes
   * - Incremental compressed backups
     - Yes
     -
   * - Fast incremental backups [#n-2]_
     - Yes
     -
   * - Incremental backups with archived logs feature in Percona Server
     - Yes
     -
   * - Incremental backups with REDO log only
     -
     - Yes
   * - Backup locks [#n-8]_
     - Yes
     -
   * - Encrypted backups
     - Yes
     - Yes [#n-3]_
   * - Streaming backups
     - Yes
     - Yes
   * - Parallel local backups
     - Yes
     - Yes
   * - Parallel compression
     - Yes
     - Yes
   * - Parallel encryption
     - Yes
     - Yes
   * - Parallel apply-log
     - Yes
     -
   * - Parallel copy-back
     -
     - Yes
   * - Partial backups
     - Yes
     - Yes
   * - Partial backups of individual partitions
     - Yes
     -
   * - Throttling [#n-4]_
     - Yes
     - Yes
   * - Backup image validation
     -
     - Yes
   * - Point-in-time recovery support
     - Yes
     - Yes
   * - Safe slave backups
     - Yes
     -
   * - Compact backups [#n-5]_
     - Yes
     -
   * - Buffer pool state backups
     - Yes
     -
   * - Individual tables export
     - Yes
     - Yes [#n-6]_
   * - Individual partitions export
     - Yes
     -
   * - Restoring tables to a different server [#n-7]_
     - Yes
     - Yes
   * - Data & index file statistics
     - Yes
     -
   * - InnoDB secondary indexes defragmentation
     - Yes
     -
   * - ``rsync`` support to minimize lock time
     - Yes
     -
   * - Improved ``FTWRL`` handling
     - Yes
     -
   * - Backup history table
     - Yes
     - Yes
   * - Backup progress table
     -
     - Yes
   * - Offline backups
     -
     - Yes
   * - Backup to tape media managers
     -
     - Yes
   * - Cloud backups support
     -
     - Amazon S3
   * - External graphical user interfaces to backup/recovery
     - Zmanda Recovery Manager for MySQL
     - MySQL Workbench, MySQL Enterprise Monitor

What are the features of Percona XtraBackup?
============================================

Here is a short list of |Percona XtraBackup| features. See the documentation
for more.

* Create hot |InnoDB| backups without pausing your database
* Make incremental backups of |MySQL|
* Stream compressed |MySQL| backups to another server
* Move tables between |MySQL| servers on-line
* Create new |MySQL| replication slaves easily
* Backup |MySQL| without adding load to the server



.. rubric:: Footnotes

.. [#n-1] |InnoDB| tables are still locked while copying non-|InnoDB| data.

.. [#n-2] Fast incremental backups are supported for |Percona Server| with
          XtraDB changed page tracking enabled.

.. [#n-3] |Percona XtraBackup| supports encryption with any kinds of backups.
          *MySQL Enterprise Backup* only supports encryption for single-file
          backups.

.. [#n-4] |Percona XtraBackup| performs throttling based on the number of IO
          operations per second. *MySQL Enterprise Backup* supports a
          configurable sleep time between operations.

.. [#n-5] |Percona XtraBackup| skips secondary index pages and recreates them
          when a compact backup is prepared. *MySQL Enterprise Backup* skips
          unused pages and reinserts on the prepare stage.

.. [#n-6] |Percona XtraBackup| can export individual tables even from a full
          backup, regardless of the InnoDB version. *MySQL Enterprise Backup*
          uses InnoDB 5.6 transportable tablespaces only when performing a
          partial backup.

.. [#n-7] Tables exported with |Percona XtraBackup| can be imported into
          |Percona Server| 5.1, 5.5 or 5.6+, or |MySQL| 5.6+. Transportable
          tablespaces created with *MySQL Enterprise Backup* can only be
          imported to |Percona Server| 5.6+, |MySQL| 5.6+ or |MariaDB| 10.0+.

	  .. include:: .res/contents/important.mariadb-support.txt	  

.. [#n-8] Backup locks is a lightweight alternative to ``FLUSH TABLES WITH READ
          LOCK`` available in |Percona Server| 5.6+. |Percona XtraBackup| uses
          them automatically to copy non-InnoDB data to avoid blocking DML
          queries that modify |InnoDB| tables.

..  [#n-9] |Percona XtraBackup| 2.4 only supports |Percona XtraDB Cluster| 5.7.
