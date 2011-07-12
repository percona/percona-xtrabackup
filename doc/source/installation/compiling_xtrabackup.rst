===========================================
 Compiling and Installing from Source Code
===========================================

The source code is available from the *Launchpad* project `here <https://launchpad.net/percona-xtrabackup>`_. The easiest way to get the code is with :command:`bzr branch` of the desired release, such as the following: ::

  bzr branch lp:percona-xtrabackup/1.6

or ::

  bzr branch lp:percona-xtrabackup

You should then have a directory named after the release you branched, such as ``percona-xtrabackup``.


Compiling on Linux
==================

Prerequisites
-------------

The following packages and tools must be installed to compile *Percona XtraBackup* from source. These might vary from system to system.

In Debian-based distributions, you need to: ::

  $ apt-get install build-essential flex bison automake autoconf bzr \
    libtool cmake libaio-dev mysql-client libncurses-dev zlib1g-dev

In ``RPM``-based distributions, you need to: ::

  $ yum install cmake gcc gcc-c++ libaio libaio-devel automake autoconf bzr \
    bison libtool ncurses5-devel

Compiling with :command:`build.sh`
----------------------------------

Once you have all dependencies met, the compilation is straight-forward with the bundled :command:`build.sh` script in the :file:`utils/` directory of the distribution.

The script needs the codebase for which the building is targeted, you must provide it with one of the following values or aliases:

  ================== =========  ============================================
  Value              Alias      Server
  ================== =========  ============================================
  innodb51_builtin   5.1	build against built-in InnoDB in MySQL 5.1
  innodb51           plugin	build agsinst InnoDB plugin in MySQL 5.1
  innodb55           5.5	build against InnoDB in MySQL 5.5
  xtradb51           xtradb     build against Percona Server with XtraDB 5.1
  xtradb55           xtradb55   build against Percona Server with XtraDB 5.5
  ================== =========  ============================================

Note that the script must be executed from the base directory of |Xtrabackup| sources, and that directory must contain the packages with the source code of the codebase selected. This may appear cumbersome, but if the variable ``AUTO_LOAD="yes"`` is set, the :command:`build.sh` script will download all the source code needed for the build.

At the base directory of the downloaded source code, if you execute ::

  $ AUTO_DOWNLOAD="yes" ./utils/build.sh xtradb

and you go for a coffee, at your return |XtraBackup| will be ready to be used. The |xtrabackup| binary will located in the following subdirectory depending on the building target:

  ================== =====================================================
  Target             Location
  ================== =====================================================
  innodb51_builtin   mysql-5.1/storage/innobase/xtrabackup
  innodb51           mysql-5.1/storage/innodb_plugin/xtrabackup
  innodb55           mysql-5.5/storage/innobase/xtrabackup
  xtradb51           Percona-Server-5.1/storage/innodb_plugin/xtrabackup
  xtradb55           Percona-Server-5.5/storage/innobase/xtrabackup
  ================== =====================================================
