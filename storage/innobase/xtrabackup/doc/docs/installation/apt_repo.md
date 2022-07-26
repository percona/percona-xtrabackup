# Installing Percona XtraBackup on Debian and Ubuntu

Ready-to-use packages are available from the *Percona XtraBackup* software
repositories and
the [download page](https://www.percona.com/downloads/XtraBackup/).

Specific information on the supported platforms, products, and versions is
described
in [Percona Release Lifecycle Overview](https://www.percona.com/services/policies/percona-software-platform-lifecycle#mysql)
.

## What’s in each DEB package?

The `percona-xtrabackup-80` package contains the latest *Percona
XtraBackup*
GA binaries and associated files.

The `percona-xtrabackup-dbg-80` package contains the debug symbols for
binaries in `percona-xtrabackup-80`.

The `percona-xtrabackup-test-80` package contains the test suite for
*Percona XtraBackup*.

The `percona-xtrabackup` package contains the older version of the
*Percona XtraBackup*.

## Installing *Percona XtraBackup* via *percona-release*

*Percona XtraBackup*, like many other *Percona* products, is installed
with the *percona-release* package configuration tool.

1. Download a deb package for *percona-release* the repository packages
   from Percona web:

```
$ wget https://repo.percona.com/apt/percona-release_latest.$(lsb_release -sc)_all.deb
```

2. Install the downloaded package with **dpkg**. To do that, run the
   following commands as root or with **
   sudo**: `dpkg -i percona-release_latest.$(lsb_release -sc)_all.deb`

Once you install this package the Percona repositories should be added. You
can check the repository setup in the
`/etc/apt/sources.list.d/percona-release.list` file.

3. Enable the repository: `percona-release enable-only tools release`

If *Percona XtraBackup* is intended to be used in combination with
the upstream MySQL Server, you only need to enable the `tools`
repository: `percona-release enable-only tools`.

4. Remember to update the local cache: `apt update`


5. After that you can install the `percona-xtrabackup-80` package:

```
$ sudo apt install percona-xtrabackup-80
```

6. In order to make compressed backups, install the `qpress` package:

```
$ sudo apt install qpress
```

**NOTE**: For AppArmor profile information, see [Working with AppArmor](https://docs.percona.com/percona-xtrabackup/8.0/security/pxb-apparmor.html).

## Apt-Pinning the packages

In some cases you might need to “pin” the selected packages to avoid the
upgrades from the distribution repositories. Make a new file
`/etc/apt/preferences.d/00percona.pref` and add the following lines in
it:

```
Package: *
Pin: release o=Percona Development Team
Pin-Priority: 1001
```

For more information about the pinning, check the official
[debian wiki](http://wiki.debian.org/AptPreferences).

## Installing *Percona XtraBackup* using downloaded deb packages

Download the packages of the desired series for your architecture
from [Download Percona XtraBackup 8.0](https://www.percona.com/downloads/XtraBackup/)
. The following
example downloads *Percona XtraBackup* 8.0.26-18 release package for Ubuntu
20.04:

```
$ wget https://downloads.percona.com/downloads/Percona-XtraBackup-LATEST/Percona-XtraBackup-8.0.26-18/binary/debian/focal/x86_64/percona-xtrabackup-80_8.0.26-18-1.focal_amd64.deb
```

Install *Percona XtraBackup* by running:

```
$ sudo dpkg -i percona-xtrabackup-80_8.0.26-18-1.focal_amd64.deb
```

**NOTE**: When installing packages manually like this, resolve all the
dependencies and install missing packages yourself.

## Update the Curl utility in Debian 10

The default curl version, 7.64.0, in Debian 10 has known issues when
attempting to reuse an already closed connection. This issue directly
affects `xbcloud` and users may see intermittent backup failures.

For more details,
see [curl #3750](https://github.com/curl/curl/issues/3750)
or [curl #3763](https://github.com/curl/curl/pull/3763).

Follow these steps to upgrade curl to version 7.74.0:

1. Edit the `/etc/apt/sources.list` to add the following:

```
deb http://ftp.de.debian.org/debian buster-backports main
```

2. Refresh the `apt` sources:

```
sudo apt update
```

3. Install the version from `buster-backports`:

```
$ sudo apt install curl/buster-backports
```

4. Verify the version number:

```
$ curl --version
curl 7.74.0 (x86_64-pc-linux-gnu) libcurl/7.74.0
```

## Uninstalling *Percona XtraBackup*

To uninstall *Percona XtraBackup*, remove all the installed
packages.

1. Remove the packages

```
$ sudo apt remove percona-xtrabackup-80
```
