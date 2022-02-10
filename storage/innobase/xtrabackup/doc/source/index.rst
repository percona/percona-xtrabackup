.. Percona XtraBackup documentation master file, created by
   sphinx-quickstart on Fri May  6 01:04:39 2011.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

====================================
 Percona XtraBackup - Documentation
====================================

*Percona XtraBackup* is an open-source hot backup utility for
*MySQL* - based servers that doesn't lock your database during the
backup.

Whether it is a 24x7 highly loaded server or a low-transaction-volume
environment, *Percona XtraBackup* is designed to make backups a seamless
procedure without disrupting the performance of the server in a production
environment. `Commercial support contracts are available
<http://www.percona.com/mysql-support/>`_.

*Percona XtraBackup* can back up data from |InnoDB|, |XtraDB|, 
|MyISAM|, and MyRocks tables on *MySQL* 8.0 servers as well as *Percona Server for MySQL*
with *XtraDB*, *Percona Server for MySQL* 8.0, and *Percona XtraDB Cluster* 8.0. 

*Percona XtraBackup* does not support backup or restore of the `MyRocks ZenFS <https://www.percona.com/doc/percona-server/LATEST/myrocks/zenfs.html>`__ data at this time

*Percona XtraBackup* 8.0 only supports making backups of the databases
created by the 8.0 versions of the *MySQL*, *Percona Server for MySQL* or
*Percona XtraDB Cluster*. The format changes that *MySQL* 8.0 introduced
in the *data dictionaries*, the *redo log*, and the *undo log* are incompatible
with previous versions of *Percona XtraBackup*.

Due to changes in *MySQL* 8.0.20, released by Oracle at the end of April 2020,
*Percona XtraBackup* 8.0, up to version 8.0.11, is not compatible with *MySQL* version 8.0.20 or higher, or the Percona products that are based on it: *Percona Server for MySQL* and *Percona XtraDB Cluster*.

For more information, see `Percona XtraBackup 8.x and MySQL 8.0.20
<https://www.percona.com/blog/2020/04/28/percona-xtrabackup-8-x-and-mysql-8-0-20/>`_

For a high-level overview of many of its advanced features, including
a feature comparison, please see :doc:`intro`.


Introduction
============

.. toctree::
   :maxdepth: 1
   :glob:

   intro
   how_xtrabackup_works


Install and uninstall
==================================

.. toctree::
   :maxdepth: 1
   :glob:

   installation/apt_repo
   security/pxb-apparmor
   installation/yum_repo
   security/pxb-selinux
   installation/binary-tarball
   installation/compiling_xtrabackup 


Run in Docker
======================================

.. toctree::
   :maxdepth: 1
   :glob:

   installation/docker

Implementation details
================================

.. toctree::
   :maxdepth: 1
   :glob:

   xtrabackup_bin/implementation_details   
   howtos/enabling_tcp
   prerequisites/connections
   prerequisites/configuring
   prerequisites/permissions
   xtrabackup-files

Option reference
=========================

.. toctree::
   :maxdepth: 1
   :glob:

   xtrabackup_bin/xbk_option_reference
   prerequisites/comparison
   xtrabackup_bin/xtrabackup_exit_codes
   advanced/locks
   xtrabackup_bin/backup.history

Advanced features
=================

.. toctree::
   :maxdepth: 1
   :glob:

   advanced/throttling_backups
   advanced/encrypted_innodb_tablespace_backups
   xtrabackup_bin/backup.encrypting
   xtrabackup_bin/lru_dump
   xtrabackup_bin/point-in-time-recovery
   xtrabackup_bin/working_with_binary_logs


Auxiliary guides
==============================

.. toctree:: 
   :maxdepth: 1
   :glob:

   howtos/enabling_tcp
   howtos/ssh_server
   xtrabackup_bin/analyzing_table_statistics
   xtrabackup_bin/flush-tables-with-read-lock
   advanced/locks
   advanced/page_tracking


Common backup scenarios
=============================

.. toctree::
   :maxdepth: 1
   :glob:

   manual
   backup_scenarios/full_backup
   backup_scenarios/incremental_backup
   backup_scenarios/compressed_backup

How to use **Percona XtraBackup**
=======================================

.. toctree::
   :maxdepth: 1
   :glob:

   howtos/recipes_xbk_full
   howtos/recipes_xbk_inc
   xtrabackup_bin/incremental_backups
   xtrabackup_bin/partial_backups
   howtos/recipes_xbk_compressed
   howtos/recipes_xbk_partition
   xtrabackup_bin/restoring_individual_tables



**xbstream** binary
===========================

.. toctree:: 
   :maxdepth: 1
   :glob:

   xbstream/xbstream
   xtrabackup_bin/backup.accelerating



How To use the **xbstream** binary
=====================================

.. toctree:: 
   :maxdepth: 1
   :glob:

   xtrabackup_bin/backup.streaming
   howtos/recipes_xbk_stream


**xbcloud** binary
====================================

.. toctree::
   :maxdepth: 1
   :glob:

   xbcloud/xbcloud
   xbcloud/xbcloud_exbackoff

How to use the **xbcloud** binary
==================================

.. toctree:: 
   :maxdepth: 1
   :glob:

   xbcloud/xbcloud_swift
   xbcloud/xbcloud_s3
   xbcloud/xbcloud_minio
   xbcloud/xbcloud_gcs
   xbcloud/xbcloud_exbackoff
   xbcloud/xbcloud_azure




**xbcrypt** binary
==========================

.. toctree:: 
   :maxdepth: 1
   :glob:

   xbcrypt/xbcrypt

How to use **Percona XtraBackup** in a cluster
===================================================

.. toctree::
   :maxdepth: 1
   :glob:

   howtos/setting_up_replication
   howtos/backup_verification
   howtos/recipes_ibkx_gtid
   xtrabackup_bin/replication
   howtos/backup_verification
   howtos/recipes_ibkx_gtid

Release notes
=========================

.. toctree::
   :maxdepth: 1
   :glob:

   release-notes

References
==========

..   known_issues

.. toctree::
   :maxdepth: 1
   :glob:

   faq
   glossary
   trademark-policy
   Version checking <version-check>

Indices and tables
==================

* :ref:`genindex`

* :ref:`search`

.. rubric:: Footnotes


