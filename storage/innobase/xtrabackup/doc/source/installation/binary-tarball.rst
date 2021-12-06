.. _tarball:

Install from a Binary Tarball
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