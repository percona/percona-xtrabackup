# Installing Percona XtraBackup from a Binary Tarball

**Percona** provides binary tarballs of **Percona XtraBackup**. Binary tarballs
contain precompiled executable files, libraries, and other dependencies and are
compressed `tar` archives. Extract the binary tarballs to any path.

Binary tarballs are available for [download](https://www.percona.com/downloads/Percona-XtraBackup-LATEST/) and installation.

The following table lists the tarball types available in `Linux - Generic`. Select the *Percona XtraBackup* 8.0 version, the software or the operating system, and the type of tarball for your installation. Binary tarballs support all distributions.

After you have downloaded the binary tarballs, extract the tarball in the file location of your choice.

| Type         | Name                                                                      | Operating systems                                        | Description                                                                            |
|--------------|---------------------------------------------------------------------------|----------------------------------------------------------|----------------------------------------------------------------------------------------|
| Full         | percona-xtrabackup-<version number>-Linux.x86_64.glibc2.12.tar.gz         | Built for CentOS 6                                       | Contains binary files, libraries, test files, and debug symbols                        |
| Minimal      | percona-xtrabackup-<version number>-Linux.x86_64.glibc2.12-minimal.tar.gz | Built for CentOS 6                                       | Contains binary files, and libraries but does not include test files, or debug symbols |                                                                           |                                                          |                                                                          |
| Full         | percona-xtrabackup-<version number>-Linux.x86_64.glibc2.17.tar.gz         | Compatible with any operating system except for CentOS 6 | Contains binary files, libraries, test files, and debug symbols                        |
| Minimal      | percona-xtrabackup-<version number>-Linux.x86_64.glibc2.17-minimal.tar.gz | Compatible with any operating system except for CentOS 6 | Contains binary files, and libraries but does not include test files, or debug symbols |                                                                           |                                                          |                                                                                        |

Select a different software, such as Ubuntu 20.04 (Focal Fossa), for a 
tarball for that operating system. You can download the packages together or separately.

The following command is an example of downloading the full tarball for 
Linux/Generic:

``` bash
$ wget https://downloads.percona.com/downloads/Percona-XtraBackup-LATEST/Percona-XtraBackup-8.0.23-16/binary/tarball/percona-xtrabackup-8.0.23-16-Linux-x86_64.glibc2.17.tar.gz
```
