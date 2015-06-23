.. Percona XtraBackup documentation master file, created by
   sphinx-quickstart on Fri May  6 01:04:39 2011.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

====================================
 Percona XtraBackup - Documentation
====================================

|Percona XtraBackup| is an open-source hot backup utility for |MySQL| - based servers that doesn't lock your database during the backup.

It can back up data from |InnoDB|, |XtraDB|, and |MyISAM| tables on |MySQL| 5.1 [#n-1]_, 5.5 and 5.6 servers, as well as |Percona Server| with |XtraDB|. For a high-level overview of many of its advanced features, including a feature comparison, please see :doc:`intro`.

Whether it is a 24x7 highly loaded server or a low-transaction-volume environment, |Percona XtraBackup| is designed to make backups a seamless procedure without disrupting the performance of the server in a production environment. `Commercial support contracts are available <http://www.percona.com/mysql-support/>`_.

.. note:: 

   This release is considered **BETA** quality and it's not intended for production.

Introduction
============

.. toctree::
   :maxdepth: 1
   :glob:

   intro

Installation
============

.. toctree::
   :maxdepth: 1
   :glob:

   installation
   installation/compiling_xtrabackup

User's Manual
=============

.. toctree::
   :maxdepth: 2
   :glob:

   manual

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

Miscellaneous
=============

.. toctree::
   :maxdepth: 1
   :glob:

   known_issues
   faq
   release-notes
   glossary
   xtrabackup-files
   trademark-policy

Indices and tables
==================

* :ref:`genindex`

* :ref:`search`

.. rubric:: Footnotes

.. [#n-1] Support for InnoDB 5.1 builtin has been removed in |Percona XtraBackup| 2.1

