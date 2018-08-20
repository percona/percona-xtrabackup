.. _installation:

===================================
Installing |Percona XtraBackup| 2.4
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

Compiling and Installing from Source Code
=========================================

|Percona XtraBackup| is open source and the code is available on
`Github <https://github.com/percona/percona-xtrabackup>`_. Following guide
describes the compiling and installation process from source code.

.. toctree::
   :maxdepth: 1
   :titlesonly:

   installation/compiling_xtrabackup
