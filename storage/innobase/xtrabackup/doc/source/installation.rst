.. _installation:
.. _install:

===================================
Installing |Percona XtraBackup| 8.0
===================================

This page provides the information on how to install |Percona XtraBackup|.
Following options are available:

* :ref:`installing_from_binaries` (recommended)
* :ref:`installing_from_tarball`
* Installing |Percona XtraBackup| from Downloaded
  :ref:`rpm <standalone_rpm>` or :ref:`apt <standalone_deb>` packages
* :ref:`compiling_xtrabackup`
* :ref:`docker`

Before installing, you might want to read the :doc:`release-notes`.

.. _installing_from_binaries:

Installing |Percona XtraBackup| from Repositories
=================================================

|Percona| provides repositories for :program:`yum` (``RPM`` packages for
*Red Hat*, *CentOS* and *Amazon Linux AMI*) and :program:`apt` (:file:`.deb`
packages for *Ubuntu* and *Debian*) for software such as |Percona Server|,
|Percona XtraBackup|, and *Percona Toolkit*. This makes it easy to install and
update your software and its dependencies through your operating system's
package manager. This is the recommended way of installing |Percona XtraBackup|.

The following guides describe the installation process for using the official
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

      Supported MySQL and Percona Server for MySQL versions in |Percona XtraBackup| 2.4 and 8.0
         :ref:`intro`

.. _installing_from_tarball:

Installing |Percona XtraBackup| from a Binary Tarball
======================================================

Binary tarballs are available for `download <https://www.percona.com/downloads/Percona-XtraBackup-LATEST/>`_ and installation. Select the |Percona XtraBackup| 8.0 version, the software or the operating system, and the type of tarball for your installation.

The following table lists the tarball types available in ``Linux - Generic``.  Both types support all distributions.

.. tabularcolumns:: |p{0.10\linewidth}|p{0.30\linewidth}|p{0.60\linewidth}|

.. list-table::
   :header-rows: 1

   * - Type
     - Name
     - Description
   * - Full
     - percona-xtrabackup-<version number>-Linux.x86_64.glibc2.12.tar.gz
     - Contains binaries, libraries, test files, and debug symbols
   * - Minimal
     - percona-xtrabackup-<version number>-Linux.x86_64.glibc2.12-minimal.tar.gz
     - Contains binaries, and libraries but does not include test files, or debug symbols

Selecting a different software, such as Ubuntu 20.04 (Focal Fossa), provides a tarball for that operating system. You can download the packages together or separately. 

The following link is an example of downloading the full tarball for Linux/Generic:

.. code-block:: bash

  $ wget https://downloads.percona.com/downloads/Percona-XtraBackup-LATEST/Percona-XtraBackup-8.0.23-16/binary/tarball/percona-xtrabackup-8.0.23-16-Linux-x86_64.glibc2.17.tar.gz

Compiling and Installing from Source Code
=========================================

|Percona XtraBackup| is open source and the code is available on
`Github <https://github.com/percona/percona-xtrabackup>`_. The following guide
describes the compiling and installation process from source code.

.. toctree::
   :maxdepth: 1
   :titlesonly:

   installation/compiling_xtrabackup



Running |Percona XtraBackup| in a Docker container
================================================================================

Docker images of |Percona XtraBackup| |version| are hosted publicly on Docker
Hub at https://hub.docker.com/r/percona/percona-xtradb-cluster/.

For more information about how to use |docker|, see the `Docker Docs`_.

.. _`Docker Docs`: https://docs.docker.com/

.. note:: Make sure that you are using the latest version of Docker.
   The ones provided by the operating system package manager
   may be outdated and may cause errors.

.. toctree::
   :maxdepth: 1
   :titlesonly:

   installation/docker


.. include:: _res/replace/proper.txt
