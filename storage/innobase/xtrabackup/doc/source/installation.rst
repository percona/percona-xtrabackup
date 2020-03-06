.. _installation:
.. _install:

===================================
Installing |Percona XtraBackup| 8.0
===================================

This page provides the information on how to install |Percona XtraBackup|.
Following options are available:

* :ref:`installing_from_binaries` (recommended)
* Installing |Percona XtraBackup| from Downloaded
  :ref:`rpm <standalone_rpm>` or :ref:`apt <standalone_deb>` packages
* :ref:`compiling_xtrabackup`

Before installing, you might want to read the :doc:`release-notes`.

.. _installing_from_binaries:

Installing |Percona XtraBackup| from Repositories
=================================================

|Percona| provides repositories for :program:`yum` (``RPM`` packages for
*Red Hat*, *CentOS* and *Amazon Linux AMI*) and :program:`apt` (:file:`.deb`
packages for *Ubuntu* and *Debian*) for software such as |Percona Server|,
|Percona XtraBackup|, and *Percona Toolkit*. This makes it easy to install and
update your software and its dependencies through your operating system's
package manager. This is the recommend way of installing where possible.

Following guides describe the installation process for using the official
Percona repositories for :file:`.deb` and :file:`.rpm` packages.

.. toctree::
   :maxdepth: 1
   :titlesonly:

   installation/apt_repo
   installation/yum_repo

.. note::

   For experimental migrations from earlier database server versions,
   you will need to backup and restore using XtraBackup 2.4 and
   then use ``mysql_upgrade`` from MySQL 8.0.x

   .. seealso::

      About the support of MySQL and Percona Server versions in |Percona XtraBackup| 2.4 and 8.0
         :ref:`intro`

Compiling and Installing from Source Code
=========================================

|Percona XtraBackup| is open source and the code is available on
`Github <https://github.com/percona/percona-xtrabackup>`_. Following guide
describes the compiling and installation process from source code.

.. toctree::
   :maxdepth: 1
   :titlesonly:

   installation/compiling_xtrabackup

