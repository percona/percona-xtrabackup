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

*Percona XtraBackup* can back up data from *InnoDB*, *XtraDB*, 
*MyISAM*, and MyRocks tables on *MySQL* 8.0 servers as well as *Percona Server for MySQL*
with *XtraDB*, *Percona Server for MySQL* 8.0, and *Percona XtraDB Cluster* 8.0.

.. include:: .res/contents/important.storage-engine.txt

.. include:: .res/contents/text.pxb.8-0.txt	     

For a high-level overview of many of its advanced features, including
a feature comparison, please see :doc:`intro`.


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

How Percona XtraBackup works
==================================

.. toctree::
   :maxdepth: 1
   :glob:

   xtrabackup_bin/implementation_details
   using_xtrabackup/privileges
   using_xtrabackup/configuring
   using_xtrabackup/comparison
   xtrabackup_bin/xtrabackup_exit_codes


Backup Scenarios
================

.. toctree::
   :maxdepth: 1
   :glob:

   backup_scenarios/full_backup
   backup_scenarios/incremental_backup
   backup_scenarios/compressed_backup
   xtrabackup_bin/partial_backups

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
   advanced/encrypted_innodb_tablespace_backups
   xtrabackup_bin/backup.encrypting
   xtrabackup_bin/lru_dump
   xtrabackup_bin/point-in-time-recovery
   xtrabackup_bin/working_with_binary_logs
   advanced/log_enhancements

Security
===============

.. toctree::
   :maxdepth: 1
   :glob:

   security/pxb-selinux
   security/pxb-apparmor

.. _aux-guides:

Auxiliary guides 
=======================

.. toctree::
   :maxdepth: 1
   :glob:

   howtos/enabling_tcp
   howtos/ssh_server
   xtrabackup_bin/analyzing_table_statistics
   xtrabackup_bin/flush-tables-with-read-lock
   advanced/locks
   advanced/page_tracking

xbcloud Binary
==================

.. toctree::
   :maxdepth: 1
   :glob:

   xbcloud/xbcloud
   xbcloud/xbcloud_swift
   xbcloud/xbcloud_s3
   xbcloud/xbcloud_minio
   xbcloud/xbcloud_gcs
   xbcloud/xbcloud_exbackoff
   xbcloud/xbcloud_azure

Tutorials, Recipes, How-tos
===========================

.. * :ref:`recipes-ibk`

.. toctree::
   :maxdepth: 2
   :hidden:

   how-tos

* :ref:`recipes-xbk`
* :ref:`howtos`

  

Release notes
=============

.. toctree::
   :maxdepth: 1
   :glob:

   release-notes

Error Message Descriptions
=============================

.. toctree:: 
   :maxdepth: 1
   :glob:

   em/instant

References
==========

..   known_issues

.. toctree::
   :maxdepth: 1
   :glob:

   xtrabackup_bin/xbk_option_reference
   xbcrypt/xbcrypt
   xbstream/xbstream
   faq
   glossary
   xtrabackup-files
   trademark-policy
   Version checking <version-check>

Indices and tables
==================

* :ref:`genindex`

* :ref:`search`


