.. _compiling_xtrabackup:

=========================================
Compiling and Installing from Source Code
=========================================

The source code is available from the |Percona XtraBackup| *Github* `project
<https://github.com/percona/percona-xtrabackup>`_. The easiest way to get the
code is with :command:`git clone` and switch to the desired release branch,
such as the following:

.. code-block:: bash

  $ git clone https://github.com/percona/percona-xtrabackup.git
  $ cd percona-xtrabackup
  $ git checkout 2.4

You should then have a directory named after the release you branched, such as
``percona-xtrabackup``.

Compiling on Linux
==================

Prerequisites
-------------

The following packages and tools must be installed to compile |Percona
XtraBackup| from source. These might vary from system to system.

In Debian-based distributions, you need to:

.. code-block:: bash

 $ apt-get install build-essential flex bison automake autoconf \
    libtool cmake libaio-dev mysql-client libncurses-dev zlib1g-dev \
    libgcrypt11-dev libev-dev libcurl4-gnutls-dev vim-common


In ``RPM``-based distributions, you need to:

.. code-block:: bash

  $ yum install cmake gcc gcc-c++ libaio libaio-devel automake autoconf \
    bison libtool ncurses-devel libgcrypt-devel libev-devel libcurl-devel \
    vim-common

Compiling with CMake
--------------------

At the base directory of the source code tree, if you execute:

.. code-block:: bash

  $ cmake -DBUILD_CONFIG=xtrabackup_release -DWITH_MAN_PAGES=OFF && make -j4

and you go for a coffee, at your return |Percona XtraBackup| will be ready to
be used.

.. note::

  You can build |Percona XtraBackup| with man pages but this requires
  ``python-sphinx`` package which isn't available from that main repositories
  for every distribution. If you installed the ``python-sphinx`` package you
  need to remove the ``-DWITH_MAN_PAGES=OFF`` from previous command.

Installation
------------

The following command:

.. code-block:: bash

  $ make install

will install all |Percona XtraBackup| binaries, the |innobackupex| script and
tests to :file:`/usr/local/xtrabackup`. You can override this either with:

.. code-block:: bash

  $ make DESTDIR=... install

or by changing the installation layout with:

.. code-block:: bash

  $ cmake -DINSTALL_LAYOUT=...
