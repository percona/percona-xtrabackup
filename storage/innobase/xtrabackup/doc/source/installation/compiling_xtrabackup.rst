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
  $ git checkout 8.0

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

   $ apt install build-essential flex bison automake autoconf \
   libtool cmake libaio-dev mysql-client libncurses-dev zlib1g-dev \
   libgcrypt11-dev libev-dev libcurl4-gnutls-dev vim-common

|Percona Xtrabackup| requires GCC version 5.3 or higher. If the
version of GCC installed on your system is lower then you may need to
install and enable `the Developer Toolset 7
<https://www.softwarecollections.org/en/scls/rhscl/devtoolset-7/>`_ on
``RPM``-based distributions to make sure that you use the latest GCC
compiler and development tools.  Then, install ``cmake`` and other
dependencies:

.. code-block:: bash

   $ yum install cmake openssl-devel libaio libaio-devel automake autoconf \
   bison libtool ncurses-devel libgcrypt-devel libev-devel libcurl-devel zlib-devel \
   vim-common

.. important::

   In order to build |Percona XtraBackup| v8.0 from source, you need
   to use `cmake` version 3. In your distribution it may be available
   either as a separate package ``cmake3`` or as ``cmake``. To check,
   run ``cmake --version`` and if it does report a version 3, install
   ``cmake3`` for your system.

Compiling with CMake
--------------------------------------------------------------------------------

At the base directory of the source code tree, run `cmake` or `cmake3`. In both
cases the options you need to use are the same. The following example
demonstrates the usage of ``cmake``. If you have ``cmake3``, replace ``cmake``
with ``cmake3`` accordingly.

.. code-block:: bash

   $ cmake -DWITH_BOOST=PATH-TO-BOOST-LIBRARY -DDOWNLOAD_BOOST=ON \
   -DBUILD_CONFIG=xtrabackup_release -DWITH_MAN_PAGES=OFF -B TARGETDIR

For the ``-DWITH_BOOST`` parameter, specify a directory in your file
system to download the boost library to. The ``-B`` parameter refers to an
existing empty directory in which the source code should be built.

Before running the ``make`` command, change the working directory to the
directory that you used as the value of the ``-B`` parameter ``cmake3``.
  
.. code-block::

   $ cd TARGETDIR
   $ make

After the ``make`` completes, |Percona XtraBackup| 8.0 will be ready for further
installation with ``make install``.

.. note::

   You can build |Percona XtraBackup| with man pages but this requires
   ``python-sphinx`` package which isn't available from that main repositories
   for every distribution. If you installed the ``python-sphinx`` package you
   need to remove the ``-DWITH_MAN_PAGES=OFF`` from previous command.

Installation
------------

The following command will install all |Percona XtraBackup| binaries,
|xtrabackup| and tests to :file:`/usr/local/xtrabackup`.

.. code-block:: bash

   $ make install

 You can override this default by using the `DESTDIR` parameter.

.. code-block:: bash

   $ make DESTDIR=... install

Alternatively, you can change the installation layout:

.. code-block:: bash

   $ cmake -DINSTALL_LAYOUT=...
