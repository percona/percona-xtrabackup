.. Percona XtraBackup documentation master file, created by
   sphinx-quickstart on Fri May  6 01:04:39 2011.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

====================================
 Percona XtraBackup - Documentation
====================================

|Percona XtraBackup| is an open-source hot backup utility for
|MySQL| - based servers that doesn't lock your database during the
backup.

Whether it is a 24x7 highly loaded server or a low-transaction-volume
environment, |Percona XtraBackup| is designed to make backups a seamless
procedure without disrupting the performance of the server in a production
environment. `Commercial support contracts are available
<http://www.percona.com/mysql-support/>`_.

|Percona XtraBackup| can back up data from |InnoDB|, |XtraDB|, 
|MyISAM|, and MyRocks tables on |MySQL| 8.0 servers as well as |Percona Server|
with |XtraDB|, |Percona Server| 8.0, and |Percona XtraDB Cluster| 8.0.

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
   :maxdepth: 2
   :glob:

   installation


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

User's Manual
=============

.. toctree::
   :maxdepth: 2
   :glob:

   manual

Advanced Features
=================

..    advanced/lockless_bin-log

.. toctree::
   :maxdepth: 1
   :glob:

   advanced/throttling_backups
   advanced/encrypted_innodb_tablespace_backups

Tutorials, Recipes, How-tos
===========================

.. * :ref:`recipes-ibk`

.. toctree::
   :maxdepth: 2
   :hidden:

   how-tos

* :ref:`recipes-xbk`
* :ref:`howtos`
* :ref:`aux-guides`

References
==========

..   known_issues

.. toctree::
   :maxdepth: 1
   :glob:

   xtrabackup_bin/xbk_option_reference
   xbcloud/xbcloud
   xbcrypt/xbcrypt
   xbstream/xbstream
   faq
   release-notes
   glossary
   xtrabackup-files
   trademark-policy
   Version checking <version-check>

Indices and tables
==================

* :ref:`genindex`

* :ref:`search`

.. rubric:: Footnotes


