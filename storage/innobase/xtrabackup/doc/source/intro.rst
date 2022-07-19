.. _intro:

==========================
 About Percona XtraBackup
==========================

*Percona XtraBackup* is the world's only open-source, free *MySQL* hot backup
software that performs non-blocking backups for *InnoDB* and *XtraDB*
databases. With *Percona XtraBackup*, you can achieve the following benefits:

* Backups that complete quickly and reliably
* Uninterrupted transaction processing during backups
* Savings on disk space and network bandwidth
* Automatic backup verification
* Higher uptime due to faster restore time

*Percona XtraBackup* makes *MySQL* hot backups for all versions of Percona
Server for MySQL, and *MySQL*. It performs streaming, compressed, and incremental *MySQL*
backups.

.. important::

   With the introduction of *Percona XtraBackup* 8.0, *Percona XtraBackup* 2.4
   will continue to support MySQL and Percona Server 5.6 and 5.7 databases. Due
   to the new MySQL redo log and data dictionary formats the Percona XtraBackup
   8.0.x versions will only be compatible with MySQL 8.0.x and the upcoming
   Percona Server for MySQL 8.0.x

Percona's enterprise-grade commercial `MySQL Support
<http://www.percona.com/mysql-support/>`_ contracts include support for *Percona
XtraBackup*. We recommend support for critical production deployments. Percona XtraDB Backup supports encryption.

.. rubric:: Supported storage engines

Percona XtraBackup works with MySQL and Percona Server. It supports
completely non-blocking backups of InnoDB, XtraDB, and MyRocks storage
engines. Fast incremental backups are supported for Percona Server with the XtraDB changed page tracking enabled.

In addition, it can back up the following storage engines by briefly
pausing writes at the end of the backup: MyISAM and :term:`Merge <.MRG>`, including partitioned tables, triggers, and database
options. InnoDB tables are still locked while copying non-InnoDB data.

.. important::

   The support of the MyRocks storage engine was added in version 8.0.6.
   Incremental backups on the MyRocks storage engine do not determine if an earlier full backup or incremental backup contains the same files. **Percona XtraBackup** copies all of the MyRocks files each time it takes a backup.

   Percona XtraBackup 8.0 does not support the TokuDB storage engine.

   .. seealso::

      Percona TokuBackup
         https://www.percona.com/doc/percona-server/LATEST/tokudb/toku_backup.html


What are the features of Percona XtraBackup?
============================================

Here is a short list of the Percona XtraBackup features. See the documentation
for more.

* Create hot InnoDB backups without pausing your database
* Make incremental backups of MySQL
* Stream compressed MySQL backups to another server
* Move tables between MySQL servers on-line
* Create new MySQL replication replicas easily
* Backup MySQL without adding load to the server
* Percona XtraBackup performs throttling based on the number of IO operations per second
* Percona XtraBackup skips secondary index pages and recreates them when a compact backup is prepared
* Percona XtraBackup can export individual tables even from a full backup, regardless of the InnoDB version
* Backup locks is a lightweight alternative to ``FLUSH TABLES WITH READ LOCK`` available in Percona Server. Percona XtraBackup uses them automatically to copy non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.

.. seealso::

     For more information, see :ref:`how_xtrabackup_works`.




.. rubric:: Additional information

*InnoDB* tables are still locked while copying non-*InnoDB* data.
