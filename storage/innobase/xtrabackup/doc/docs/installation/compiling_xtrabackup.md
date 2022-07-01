# Compiling and Installing from Source Code

The source code is available from the *Percona XtraBackup* *Github* [project](https://github.com/percona/percona-xtrabackup). The easiest way to get the
code is by using the `git clone` command. Then, switch to the release
branch that you want to install, such as **2.4**.

```bash
$ git clone https://github.com/percona/percona-xtrabackup.git
$ cd percona-xtrabackup
$ git checkout 2.4
```

## Step 1: Installing prerequisites

The following packages and tools must be installed to compile *Percona
XtraBackup* from source. These might vary from system to system.

!!! important

    In order to build *Percona XtraBackup* v8.0 from source, you need to use `cmake` version 3. In your distribution, it may be available either as a separate package ``cmake3`` or as ``cmake``. To check which version is installed, run ``cmake --version`` and if it does report a version 3, install ``cmake3`` for your system.

!!! seealso

    https://cmake.org/

### Debian or Ubuntu using `apt`

```bash
$ sudo apt install build-essential flex bison automake autoconf \
libtool cmake libaio-dev mysql-client libncurses-dev zlib1g-dev \
libgcrypt11-dev libev-dev libcurl4-gnutls-dev vim-common
```

To install the man pages, install the `python3-sphinx` package:

```bash
$ sudo apt install python3-sphinx
```

### CentOS or Red Hat using `yum`

Percona Xtrabackup requires GCC version 5.3 or higher. If the
version of GCC installed on your system is lower then you may need to
install and enable [the Developer Toolset 7](https://www.softwarecollections.org/en/scls/rhscl/devtoolset-7/) on
`RPM`-based distributions to make sure that you use the latest GCC
compiler and development tools.  Then, install `cmake` and other
dependencies:

```bash
$ sudo yum install cmake openssl-devel libaio libaio-devel automake autoconf \
bison libtool ncurses-devel libgcrypt-devel libev-devel libcurl-devel zlib-devel \
vim-common
```

To install the man pages, install the `python3-sphinx` package:

```bash
$ sudo yum install python3-sphinx
```

## Step 2: Generating the build pipeline

At this step, you have `cmake` run the commands in the `CMakeList.txt`
file to generate the build pipeline, i.e. a native build environment that will
be used to compile the source code).

1. Change to the directory where you cloned the Percona XtraBackup repository

```bash
$ cd percona-xtrabackup
```

2. Create a directory to store the compiled files and then change to that
directory:

```bash
$ mkdir build
$ cd build
```

3. Run `cmake` or `cmake3`. In either case, the options you need to use are the
same.

!!! note

    You can build *Percona XtraBackup* with man pages but this requires `python-sphinx` package which isn’t available from that main repositories for every distribution. If you installed the `python-sphinx` package you need to remove the `-DWITH_MAN_PAGES=OFF` from previous command. 
    
    ```bash 
    $ cmake -DWITH_BOOST=PATH-TO-BOOST-LIBRARY -DDOWNLOAD_BOOST=ON \
    -DBUILD_CONFIG=xtrabackup_release -DWITH_MAN_PAGES=OFF -B ..
    ``` 

### More information about parameters

`-DWITH_BOOST`

For the `-DWITH_BOOST` parameter, specify the name of a directory to download the boost library to. This directory will be created automatically in your current directory.

`-B (--build)`

*Percona XtraBackup* is configured to forbid generating the build pipeline for ``make`` in the same directory where you store your sources. The ``-B`` parameter refers to the directory that contains the source code. In this example we use the relative path to the parent directory (..).

!!! important

    CMake Error at CMakeLists.txt:367 (MESSAGE): Please do not build in-source.  Out-of source builds are highly recommended: you can have multiple builds for the same source, and there is an easy way to do cleanup, simply remove the build directory (note that 'make clean' or 'make distclean' does *not* work)

    You *can* force in-source build by invoking cmake with -DFORCE_INSOURCE_BUILD=1

`-DWITH_MAN_PAGES`

To build *Percona XtraBackup* man pages, use ``ON`` or remove this parameter from the command line (it is ``ON`` by default). 

To install the man pages, install the `python3-sphinx` package:

## Step 2: Compiling the source code

To compile the source code in your build directory, use the `make` command.

!!! important

    The computer where you intend to compile *Percona XtraBackup* 8.0 must have at least 2G of RAM available.

1. Change to the `build` directory (created at [Step 2: Generating the build pipeline](compiling_xtrabackup.md#pxb-source-code-installing-build-pipe-line-generating)).

2. Run the `make` command. This command may take a long time to complete.

```bash
$ make
```

## Step 3: Installing on the target system

The following command installs all *Percona XtraBackup* binaries *xtrabackup*
and tests to default location on the target system: `/usr/local/xtrabackup`.

Run `make install` to install *Percona XtraBackup* to the default location.

```bash
$ sudo make install
```

### Installing to a non-default location

You may use the `DESTDIR` parameter with `make install` to install *Percona XtraBackup* to another location. Make sure that the effective user is able to
write to the destination you choose.

```bash
$ sudo make DESTDIR=<DIR_NAME> install
```

In fact, the destination directory is determined by the installation layout
(`-DINSTALL_LAYOUT`) that `cmake` applies (see
Step 2: Generating the build pipeline). In addition to
the installation directory, this parameter controls a number of other
destinations that you can adjust for your system.

By default, this parameter is set to `STANDALONE`, which implies the
installation directory to be /usr/local/xtrabackup.

!!! seealso

    [MySQL Documentation: -DINSTALL_LAYOUT](https://dev.mysql.com/doc/refman/8.0/en/source-configuration-options.html#option_cmake_install_layout)

## Step 4: Running

After *Percona XtraBackup* is installed on your system, you may run it by using
the full path to the `xtrabackup` command:

```bash
$ /usr/local/xtrabackup/bin/xtrabackup
```

Update your PATH environment variable if you would like to use the command on
the command line directly.

```bash
$# Setting $PATH on the command line
$ PATH=$PATH:/usr/local/xtrabackup/bin/xtrabackup

$# Run xtrabackup directly
$ xtrabackup
```

Alternatively, you may consider placing a soft link (using `ln -s`) to one of
the locations listed in your `PATH` environment variable.

!!! seealso

    ``man ln``

To view the documentation with `man`, update the `MANPATH` variable.