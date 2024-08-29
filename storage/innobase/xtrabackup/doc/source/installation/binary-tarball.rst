.. _binary-tarball:

========================================================
Installing *Percona XtraBackup* from a Binary Tarball
========================================================

**Percona** provides binary tarballs of **Percona XtraBackup**. Binary tarballs
contain precompiled executable files, libraries, and other dependencies and are
compressed ``tar`` archives. Extract the binary tarballs to any path.

Binary tarballs are available for `download <https://www.percona.com/downloads/Percona-XtraBackup-LATEST/>`__ and installation.

The following table lists the tarball types available in ``Linux - Generic``. Select the *Percona XtraBackup* 8.0 version, the software or the operating system, and the type of tarball for your installation. Binary tarballs support all distributions.

After you have downloaded the binary tarballs, extract the tarball in the file location of your choice.

.. important::

    Starting with **Percona XtraBackup 8.0.28-20**, Percona no longer provides a
    tarball for CentOS 6. For more information, see `Spring Cleaning:
    Discontinuing RHEL 6/CentOS 6 (glibc2.12) and 32-bit Binary Builds of
    Percona Software
    <https://www.percona.com/blog/spring-cleaning-discontinuing-rhel-6-centos-6-glibc-2-12-and-32-bit-binary-builds-of-percona-software/>`__

.. tabularcolumns:: |p{0.08\linewidth}|p{0.30\linewidth}|p{0.10\linewidth}|p{0.40\linewidth}|

.. list-table::
   :header-rows: 1

   * - Type
     - Name
     - Operating systems
     - Description
   * - Full
     - percona-xtrabackup-<version number>-Linux.x86_64.glibc2.12.tar.gz
     - Built for CentOS 6
     - Contains binary files, libraries, test files, and debug symbols
   * - Minimal
     - percona-xtrabackup-<version number>-Linux.x86_64.glibc2.12-minimal.tar.gz
     - Built for CentOS 6
     - Contains binary files, and libraries but does not include test files, or
       debug symbols
   * - Full
     - percona-xtrabackup-<version number>-Linux.x86_64.glibc2.17.tar.gz
     - Compatible with any operating system except for CentOS 6
     - Contains binary files, libraries, test files, and debug symbols
   * - Minimal
     - percona-xtrabackup-<version number>-Linux.x86_64.glibc2.17-minimal.tar.gz
     - Compatible with any operating system except for CentOS 6
     - Contains binary files, and libraries but does not include test files, or
       debug symbols



Selecting a different software, such as Ubuntu 20.04 (Focal Fossa), provides a tarball for that operating system. You can download the packages together or separately. 

The following link is an example of downloading the full tarball for Linux/Generic:

.. code-block:: bash

  $ wget https://downloads.percona.com/downloads/Percona-XtraBackup-LATEST/Percona-XtraBackup-8.0.23-16/binary/tarball/percona-xtrabackup-8.0.23-16-Linux-x86_64.glibc2.17.tar.gz
