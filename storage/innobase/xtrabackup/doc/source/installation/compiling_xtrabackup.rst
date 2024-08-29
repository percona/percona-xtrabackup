.. _compiling_xtrabackup:

================================================================================
Compiling and Installing from Source Code
================================================================================

The source code is available from the *Percona XtraBackup Github* `project
<https://github.com/percona/percona-xtrabackup>`_. The easiest way to get the
code is by using the :command:`git clone` command. Then, switch to the release
branch that you want to install, such as **8.0**.

.. code-block:: bash

  $ git clone https://github.com/percona/percona-xtrabackup.git
  $ cd percona-xtrabackup
  $ git checkout 8.0
  $ git submodule update --init --recursive

.. _pxb.source-code.installing/prerequesite:

Step 1: Installing prerequisites
================================================================================

The following packages and tools must be installed to compile *Percona XtraBackup* from source. 
These might vary from system to system.

.. important::

   In order to build *Percona XtraBackup* v8.0 from source, you must use
   `cmake` version 3. In your distribution, it may be available either as a
   separate package ``cmake3`` or as ``cmake``. To check which version is
   installed, run ``cmake --version`` and if it does report a version 3, install
   ``cmake3`` for your system.

   .. seealso:: https://cmake.org/

.. rubric:: Debian or Ubuntu using ``apt``

.. code-block:: bash

      sudo apt install bison pkg-config cmake devscripts debconf \
      debhelper automake bison ca-certificates \
      libcurl4-openssl-dev cmake debhelper libaio-dev \
      libncurses-devlibssl-dev libtool libz-dev libgcrypt-dev libev-dev \
      lsb-release python-docutils build-essential rsync \
      libdbd-mysql-perl libnuma1 socat librtmp-dev libtinfo5 \ 
      qpress liblz4-tool liblz4-1 liblz4-dev vim-common

To install the man pages, install the python3-sphinx package first:

.. code-block:: bash

   $ sudo apt install python3-sphinx

.. rubric:: CentOS or Red Hat using ``yum``

*Percona Xtrabackup* requires GCC version 5.3 or higher. If the
version of GCC installed on your system is lower then you may need to
install and enable `the Developer Toolset 7
<https://www.softwarecollections.org/en/scls/rhscl/devtoolset-7/>`_ on
``RPM``-based distributions to make sure that you use the latest GCC
compiler and development tools.  Then, install ``cmake`` and other
dependencies:

.. code-block:: bash

   $ sudo yum install cmake openssl-devel libaio libaio-devel automake autoconf \
   bison libtool ncurses-devel libgcrypt-devel libev-devel libcurl-devel zlib-devel \
   vim-common

To install the man pages, install the python3-sphinx package first:

.. code-block:: bash

   $ sudo yum install python3-sphinx

.. _pxb.source-code.installing/build-pipe-line.generating:

Step 2: Generating the build pipeline
================================================================================

At this step, you have ``cmake`` run the commands in the :file:`CMakeList.txt`
file to generate the build pipeline, i.e. a native build environment that will
be used to compile the source code).

1. Change to the directory where you cloned the Percona XtraBackup repository 

   .. code-block:: bash

      $ cd percona-xtrabackup

#. Create a directory to store the compiled files and then change to that
   directory:

   .. code-block:: bash

      $ mkdir build
      $ cd build

#. Run `cmake` or `cmake3`. In either case, the options you need to use are the
   same. 

.. note::

   You can build *Percona XtraBackup* with man pages but this requires
   ``python-sphinx`` package which isn't available from that main repositories
   for every distribution. If you installed the ``python-sphinx`` package you
   need to remove the ``-DWITH_MAN_PAGES=OFF`` from previous command.


   .. code-block:: bash

      $ cmake -DWITH_BOOST=PATH-TO-BOOST-LIBRARY -DDOWNLOAD_BOOST=ON \
      -DBUILD_CONFIG=xtrabackup_release -DWITH_MAN_PAGES=OFF -B ..

   .. admonition:: More information about parameters

      -DWITH_BOOST
         For the ``-DWITH_BOOST`` parameter, specify the name of a directory to
	 download the boost library to. This directory will be created automatically
	 in your current directory.

      -B (--build)
         *Percona XtraBackup* is configured to forbid generating the build pipeline for
	 ``make`` in the same directory where you store your sources. The ``-B``
	 parameter refers to the directory that contains the source code. In
	 this example we use the relative path to the parent directory (..).

	 .. important::

	    CMake Error at CMakeLists.txt:367 (MESSAGE): Please do not build
	    in-source.  Out-of source builds are highly recommended: you can
	    have multiple builds for the same source, and there is an easy way
	    to do cleanup, simply remove the build directory (note that 'make
	    clean' or 'make distclean' does *not* work)

	    You *can* force in-source build by invoking cmake with
	    -DFORCE_INSOURCE_BUILD=1

      -DWITH_MAN_PAGES
         To build *Percona XtraBackup* man pages, use ``ON`` or remove this
	 parameter from the command line (it is ``ON`` by default).

	 To install the man pages, install the python3-sphinx package first.

	 .. seealso:: :ref:`pxb.source-code.installing/prerequesite`

.. _pxb.source-code.installing/compiling:

Step 2: Compiling the source code
================================================================================

To compile the source code in your :file:`build` directory, use the ``make`` command.

.. important::
   
   The computer where you intend to compile *Percona XtraBackup* 8.0 must have
   at least 2G of RAM available.

1. Change to the :file:`build` directory (created at
   :ref:`pxb.source-code.installing/build-pipe-line.generating`).
#. Run the ``make`` command. This command may take a long time to complete.

   .. code-block:: bash

      $ make

.. _pxb.source-code.installing/target-system:

Step 3: Installing on the target system
================================================================================

The following command installs all *Percona XtraBackup* binaries *xtrabackup*
and tests to default location on the target system: :file:`/usr/local/xtrabackup`.

Run ``make install`` to install *Percona XtraBackup* to the default location.

.. code-block:: bash

   $ sudo make install

.. rubric:: Installing to a non-default location

You may use the `DESTDIR` parameter with ``make install`` to install *Percona
XtraBackup* to another location. Make sure that the effective user is able to
write to the destination you choose.

.. code-block:: bash

   $ sudo make DESTDIR=<DIR_NAME> install

In fact, the destination directory is determined by the installation layout
(``-DINSTALL_LAYOUT``) that ``cmake`` applies (see
:ref:`pxb.source-code.installing/build-pipe-line.generating`). In addition to
the installation directory, this parameter controls a number of other
destinations that you can adjust for your system.

By default, this parameter is set to ``STANDALONE``, which implies the
installation directory to be :file:`/usr/local/xtrabackup`.

.. seealso:: `MySQL Documentation: -DINSTALL_LAYOUT
             <https://dev.mysql.com/doc/refman/8.0/en/source-configuration-options.html#option_cmake_install_layout>`_

.. _pxb.source-code.installing/running:

Step 4: Running
================================================================================

After *Percona XtraBackup* is installed on your system, you may run it by using
the full path to the ``xtrabackup`` command:

.. code-block:: bash

   $ /usr/local/xtrabackup/bin/xtrabackup

Update your PATH environment variable if you would like to use the command on
the command line directly.

.. code-block:: bash

   $# Setting $PATH on the command line
   $ PATH=$PATH:/usr/local/xtrabackup/bin/xtrabackup

   $# Run xtrabackup directly
   $ xtrabackup

Alternatively, you may consider placing a soft link (using ``ln -s``) to one of
the locations listed in your ``PATH`` environment variable.

.. seealso:: ``man ln``

To view the documentation with ``man``, update the ``MANPATH`` variable.
