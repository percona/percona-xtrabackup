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

|Percona XtraBackup| makes |MySQL| hot backups for all versions of |Percona
Server|, and |MySQL|. It performs streaming, compressed, and incremental |MySQL|
backups.

.. important::

   With the introduction of |Percona XtraBackup| 8.0, |Percona XtraBackup| 2.4
   will continue to support MySQL and Percona Server 5.6 and 5.7 databases. Due
   to the new MySQL redo log and data dictionary formats the Percona XtraBackup
   8.0.x versions will only be compatible with MySQL 8.0.x and the upcoming
   Percona Server for MySQL 8.0.x

Percona's enterprise-grade commercial `MySQL Support
<http://www.percona.com/mysql-support/>`_ contracts include support for |Percona
XtraBackup|. We recommend support for critical production deployments.

.. rubric:: Supported storage engines

|Percona XtraBackup| works with |MySQL| and |Percona Server|. It supports
completely non-blocking backups of |InnoDB|, |XtraDB|, and MyRocks storage
engines. In addition, it can back up the following storage engines by briefly
pausing writes at the end of the backup: |MyISAM|, :term:`Merge <.MRG>`, and
:term:`Archive <.ARM>`, including partitioned tables, triggers, and database
options.

.. include:: .res/contents/important.storage-engine.txt   

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

.. [#n-8] Backup locks is a lightweight alternative to ``FLUSH TABLES WITH READ
          LOCK`` available in |Percona Server|. |Percona XtraBackup| uses
          them automatically to copy non-InnoDB data to avoid blocking DML
          queries that modify |InnoDB| tables.
	  
	  For more information see :ref:`how_xtrabackup_works`
