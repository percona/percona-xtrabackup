# Installing Percona XtraBackup on Red Hat Enterprise Linux and CentOS

Ready-to-use packages are available from the *Percona XtraBackup* software
repositories and the [download page](https://www.percona.com/downloads/XtraBackup/). The Percona yum repository supports popular *RPM*-based operating systems, including the *Amazon
Linux AMI*.

The easiest way to install the *Percona Yum* repository is to install an *RPM*
that configures yum and installs the [Percona GPG key](https://www.percona.com/downloads/RPM-GPG-KEY-percona).

Specific information on the supported platforms, products, and versions is described in [Percona Software and Platform Lifecycle](https://www.percona.com/services/policies/percona-software-platform-lifecycle#mysql).

## What’s in each RPM package?

The `percona-xtrabackup-80` package contains the latest *Percona XtraBackup*
GA binaries and associated files.

| Package

 | Contains

 |
| ----------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------- | -------------------------------------------------------------------------------------- |  |  |  |  |
| `percona-xtrabackup-80-debuginfo`

                                   | The debug symbols for binaries in `percona-xtrabackup-80`

                                                                                                                                                                                                                                                                                                                                                                                                                     |
| `percona-xtrabackup-test-80`

                                        | The test suite for *Percona XtraBackup*

                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| `percona-xtrabackup`

                                                | The older version of the *Percona XtraBackup*

                                                                                                                                                                                                                                                                                                                                                                                                                                 |
## Installing *Percona XtraBackup* from Percona `yum` repository


1. Install the Percona yum repository by running the following command as the
`root` user or with **sudo**: `yum install https://repo.percona.com/yum/percona-release-latest.noarch.rpm`


2. Enable the repository: `percona-release enable-only tools release`

If *Percona XtraBackup* is intented to be used in combination with
the upstream MySQL Server, you only need to enable the `tools`
repository: `percona-release enable-only tools`.


3. Install *Percona XtraBackup* by running:  `yum install percona-xtrabackup-80`

**WARNING**: Make sure that you have the `libev` package installed before
installing *Percona XtraBackup* on CentOS 6. For this operating system, the
`libev` package is available from the [EPEL](https://fedoraproject.org/wiki/EPEL) repositories.


1. To be able to make compressed backups, install the `qpress` package:

$ yum install qpress

## Installing *Percona XtraBackup* using downloaded rpm packages

Download the packages of the desired series for your architecture from the
[download page](https://www.percona.com/downloads/XtraBackup/). The following
example downloads *Percona XtraBackup* 8.0.4 release package for *CentOS*
7:

```
$ wget https://www.percona.com/downloads/XtraBackup/Percona-XtraBackup-8.0.4/binary/redhat/7/x86_64/percona-xtrabackup-80-8.0.4-1.el7.x86_64.rpm
```

Now you can install *Percona XtraBackup* by running `yum localinstall`:

```
$ yum localinstall percona-xtrabackup-80-8.0.4-1.el7.x86_64.rpm
```

**NOTE**: When installing packages manually like this, you’ll need to make sure to
resolve all the dependencies and install missing packages yourself.

## Uninstalling *Percona XtraBackup*

To completely uninstall *Percona XtraBackup* you’ll need to remove all the
installed packages: `yum remove percona-xtrabackup`
