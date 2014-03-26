.. _compiling_xtrabackup:

===========================================
 Compiling and Installing from Source Code
===========================================

The source code is available from the *Launchpad* project `here <https://launchpad.net/percona-xtrabackup>`_. The easiest way to get the code is with :command:`bzr branch` of the desired release, such as the following: ::

  bzr branch lp:percona-xtrabackup/2.2

You should then have a directory named after the release you branched, such as ``percona-xtrabackup``.


Compiling on Linux
==================

Prerequisites
-------------

The following packages and tools must be installed to compile |Percona XtraBackup| from source. These might vary from system to system.

In Debian-based distributions, you need to: ::

 $ apt-get install build-essential flex bison automake autoconf bzr \
    libtool cmake libaio-dev mysql-client libncurses-dev zlib1g-dev \
    libgcrypt11-dev


In ``RPM``-based distributions, you need to: ::
 
  $ yum install cmake gcc gcc-c++ libaio libaio-devel automake autoconf bzr \
    bison libtool ncurses5-devel

Compiling with CMake
--------------------

At the base directory of the source code tree, if you execute: ::

  $ cmake -DBUILD_CONFIG=xtrabackup_release && make -j4

and you go for a coffee, at your return |Percona XtraBackup| will be ready to be used.

Installation
------------

The following command: ::

  $ make install

will install all |Percona XtraBackup| binaries, the |innobackupex| script and tests to :file:`/usr/local/xtrabackup`. You can override this either with: :: 
  
  make DESTDIR=... install

or by changing the installation layout with: :: 

 cmake -DINSTALL_LAYOUT=...

