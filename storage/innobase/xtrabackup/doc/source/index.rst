.. Percona XtraBackup documentation master file, created by
   sphinx-quickstart on Fri May  6 01:04:39 2011.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

====================================
 Percona XtraBackup - Documentation
====================================

*Percona XtraBackup* is an open-source hot backup utility for *MySQL* - based
servers that does not lock your database during the backup. It can back up data
from *InnoDB*, XtraDB, and *MyISAM* tables on *MySQL* 5.1 , 5.5, 5.6 and 5.7 servers, as well as Percona Server with XtraDB.

.. note::

  Support for InnoDB 5.1 builtin has been removed in *Percona XtraBackup* 2.1

For a high-level overview of many of its advanced features, including a feature
comparison, please see :doc:`intro`.

Whether it is a 24x7 highly loaded server or a low-transaction-volume
environment, *Percona XtraBackup* is designed to make backups a seamless
procedure without disrupting the performance of the server in a production
environment. `Commercial support contracts are available
<https://www.percona.com/mysql-support/>`_.

.. important::

   Percona XtraBackup 2.4 does not support making backups of databases
   created in *MySQL 8.0*, *Percona Server for MySQL 8.0*, or *Percona XtraDB Cluster 8.0*.

   Use `Percona XtraBackup 8.0 <https://www.percona.com/downloads/Percona-XtraBackup-LATEST/#>`__ for making backups of databases in *MySQL 8.0*, *Percona Server for MySQL 8.0*, and *Percona XtraDB Cluster 8.0*.


Introduction
============

.. toctree::
   :maxdepth: 1
   :glob:

   intro
   how_xtrabackup_works


Installation
============

.. toctree::
   :maxdepth: 1
   :glob:

   installation/apt_repo
   installation/yum_repo
   installation/binary-tarball
   installation/compiling_xtrabackup

Run in Docker
=====================

.. toctree::
   :maxdepth: 1
   :glob:
   
   installation/docker

Prerequisites
=============

.. toctree::
   :maxdepth: 1
   :glob:

   using_xtrabackup/privileges
   using_xtrabackup/configuring

Backup Scenarios
================

.. toctree::
   :maxdepth: 1
   :glob:

   backup_scenarios/full_backup
   backup_scenarios/incremental_backup
   backup_scenarios/compressed_backup
   backup_scenarios/encrypted_backup

User's Manual
=============

.. toctree::
   :maxdepth: 2
   :glob:

   manual

Advanced Features
=================

.. toctree::
   :maxdepth: 1
   :glob:

   advanced/throttling_backups
   advanced/lockless_bin-log
   advanced/encrypted_innodb_tablespace_backups
   advanced/locks

Tutorials, Recipes, How-tos
===========================

.. toctree::
   :maxdepth: 2
   :hidden:

   how-tos

* :ref:`recipes-xbk`

* :ref:`recipes-ibk`

* :ref:`howtos`

* :ref:`aux-guides`

Release Notes
=================

.. toctree::
   :maxdepth: 1
   :glob:
   
   release-notes

References
==========

.. toctree::
   :maxdepth: 1
   :glob:


   xtrabackup_bin/xbk_option_reference
   innobackupex/innobackupex_option_reference
   xbcloud/xbcloud
   xbcloud/xbcloud_exbackoff
   xbcloud/xbcloud_azure
   xbcrypt/xbcrypt
   xbstream/xbstream
   known_issues
   faq
   glossary
   xtrabackup-files
   trademark-policy
   Version checking <version-check>

Indices and tables
==================

* :ref:`genindex`




