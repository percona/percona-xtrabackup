
# Percona XtraBackup - Documentation

*Percona XtraBackup* is an open-source hot backup utility, for
*MySQL* - based servers, that does not lock your database during the
backup.

Whether it is a 24x7 highly loaded server or a low-transaction-volume
environment, *Percona XtraBackup* is designed to make backups a seamless
procedure without disrupting the performance of the server in a production
environment. [Commercial support contracts are available](http://www.percona.com/mysql-support/).

*Percona XtraBackup* can back up data from *InnoDB*, *XtraDB*,
*MyISAM*, and MyRocks tables on *MySQL* 8.0 servers as well as *Percona Server for MySQL*
with *XtraDB*, *Percona Server for MySQL* 8.0, and *Percona XtraDB Cluster* 8.0.

**_Version Updates_**

Version 8.0.6 and later supports the MyRocks storage engine. 
An incremental backup on the MyRocks storage engine does not 
determine if an earlier full or incremental backup 
contains the same files. **Percona XtraBackup** copies all 
MyRocks files each time it takes a backup.
**Percona 
XtraBackup** does not support the TokuDB storage engine. 

_See Also_ [Percona TokuBackup](https://docs.percona.com/percona-server/latest/tokudb/toku_backup.html) 

*Percona XtraBackup* 8.0 does not support making backups of databases
created in versions prior to 8.0 of *MySQL*, *Percona Server for MySQL* or
*Percona XtraDB Cluster*. As the changes that *MySQL* 8.0 introduced
in *data dictionaries*, *redo log* and *undo log* are incompatible
with previous versions, it is currently impossible for *Percona XtraBackup* 8.0 to also support versions prior to 8.0.

Due to changes in MySQL 8.0.20 released by Oracle at the end of April 2020,
*Percona XtraBackup* 8.0, up to version 8.0.11, is not compatible with MySQL version 8.0.20 or
higher, or Percona products that are based on it: Percona Server for MySQL and
Percona XtraDB Cluster.

For more information, see [Percona XtraBackup 8.x and MySQL 8.0.20](https://www.percona.com/blog/2020/04/28/percona-xtrabackup-8-x-and-mysql-8-0-20/)

For a high-level overview of many of its advanced features, including
a feature comparison, please see [About Percona XtraBackup]
(https://docs.percona.com/percona-xtrabackup/8.0/intro.html)p.

## Introduction


* [About Percona XtraBackup](https://docs.percona.com/percona-xtrabackup/8.0/intro.html)


* [How *Percona XtraBackup* Works](https://docs.percona.com/percona-xtrabackup/8.0/how_xtrabackup_works.html)


## Installation


* [Installing *Percona XtraBackup* on *Debian* and *Ubuntu*](https://docs.percona.com/percona-xtrabackup/8.0/installation/apt_repo.html)


* [Installing *Percona XtraBackup* on *Red Hat Enterprise Linux* and *CentOS*](https://docs.percona.com/percona-xtrabackup/8.0/installation/yum_repo.html)


* [Installing *Percona XtraBackup* from a Binary Tarball](https://docs.percona.com/percona-xtrabackup/8.0/installation/binary-tarball.html)


* [Compiling and Installing from Source Code](https://docs.percona.com/percona-xtrabackup/8.0/installation/compiling_xtrabackup.html)


## Run in Docker


* [Running Percona XtraBackup in a Docker container](https://docs.percona.com/percona-xtrabackup/8.0/installation/docker.html)


## How Percona XtraBackup works


* [Implementation Details](https://docs.percona.com/percona-xtrabackup/8.0/xtrabackup_bin/implementation_details.html)


* [Connection and Privileges Needed](https://docs.percona.com/percona-xtrabackup/8.0/using_xtrabackup/privileges.html)


* [Configuring xtrabackup](https://docs.percona.com/percona-xtrabackup/8.0/using_xtrabackup/configuring.html)


* [Server Version and Backup Version Comparison](https://docs.percona.com/percona-xtrabackup/8.0/using_xtrabackup/comparison.html)


* [*xtrabackup* Exit Codes](https://docs.percona.com/percona-xtrabackup/8.0/xtrabackup_bin/xtrabackup_exit_codes.html)


## Backup Scenarios


* [The Backup Cycle - Full Backups](https://docs.percona.com/percona-xtrabackup/8.0/backup_scenarios/full_backup.html)


* [Incremental Backup](https://docs.percona.com/percona-xtrabackup/8.0/backup_scenarios/incremental_backup.html)


* [Compressed Backup](https://docs.percona.com/percona-xtrabackup/8.0/backup_scenarios/compressed_backup.html)


* [Partial Backups](https://docs.percona.com/percona-xtrabackup/8.0/xtrabackup_bin/partial_backups.html)


## User’s Manual


* [*Percona XtraBackup* User Manual](https://docs.percona.com/percona-xtrabackup/8.0/manual.html)


## Advanced Features


* [Throttling Backups](https://docs.percona.com/percona-xtrabackup/8.0/advanced/throttling_backups.html)


* [Encrypted InnoDB tablespace backups](https://docs.percona.com/percona-xtrabackup/8.0/advanced/encrypted_innodb_tablespace_backups.html)


* [Encrypting Backups](https://docs.percona.com/percona-xtrabackup/8.0/xtrabackup_bin/backup.encrypting.html)


* [LRU dump backup](https://docs.percona.com/percona-xtrabackup/8.0/xtrabackup_bin/lru_dump.html)


* [Point-In-Time recovery](https://docs.percona.com/percona-xtrabackup/8.0/xtrabackup_bin/point-in-time-recovery.html)


* [Working with Binary Logs](https://docs.percona.com/percona-xtrabackup/8.0/xtrabackup_bin/working_with_binary_logs.html)


* [Improved Log statements](https://docs.percona.com/percona-xtrabackup/8.0/advanced/log_enhancements.html)


## Security


* [Working with SELinux](https://docs.percona.com/percona-xtrabackup/8.0/security/pxb-selinux.html)


* [Working with AppArmor](https://docs.percona.com/percona-xtrabackup/8.0/security/pxb-apparmor.html)


## Auxiliary guides


* [Enabling the server to communicate via TCP/IP](https://docs.percona.com/percona-xtrabackup/8.0/howtos/enabling_tcp.html)


* [Installing and configuring an SSH server](https://docs.percona.com/percona-xtrabackup/8.0/howtos/ssh_server.html)


* [Analyzing table statistics](https://docs.percona.
  com/percona-xtrabackup/8.0/xtrabackup_bin/analyzing_table_statistics.html)


* [`FLUSH TABLES WITH READ LOCK` option](https://docs.percona.com/percona-xtrabackup/8.0/xtrabackup_bin/flush-tables-with-read-lock.html)


* [`lock-ddl-per-table` option improvements](https://docs.percona.
  com/percona-xtrabackup/8.0/advanced/locks.html)


* [Incremental backup using page tracking](https://docs.percona.com/percona-xtrabackup/8.0/advanced/page_tracking.html)


## xbcloud Binary


* [The xbcloud Binary](https://docs.percona.com/percona-xtrabackup/8.0/xbcloud/xbcloud.html)


* [Using the xbcloud Binary with Swift](https://docs.percona.com/percona-xtrabackup/8.0/xbcloud/xbcloud_swift.html)


* [Using xbcloud Binary with Amazon S3](https://docs.percona.com/percona-xtrabackup/8.0/xbcloud/xbcloud_s3.html)


* [Using the xbcloud Binary with MinIO](https://docs.percona.com/percona-xtrabackup/8.0/xbcloud/xbcloud_minio.html)


* [Using the xbcloud with Google Cloud Storage](https://docs.percona.com/percona-xtrabackup/8.0/xbcloud/xbcloud_gcs.html)


* [Exponential Backoff](https://docs.percona.com/percona-xtrabackup/8.0/xbcloud/xbcloud_exbackoff.html)


* [Using the xbcloud binary with Microsoft Azure Cloud Storage](https://docs.percona.com/percona-xtrabackup/8.0/xbcloud/xbcloud_azure.html)


## Tutorials, Recipes, How-tos

* [Recipes for xtrabackup](https://docs.percona.com/percona-xtrabackup/8.0/how-tos.html#recipes-xbk)


* [How-Tos](https://docs.percona.com/percona-xtrabackup/8.0/how-tos.html#howtos)

## Release notes


* [*Percona XtraBackup* 8.0 Release Notes](https://docs.percona.com/percona-xtrabackup/8.0/release-notes.html)

## Error message descriptions

* [Error Message: Found tables with row version due to INSTANT ADD/DROP 
  columns](https://docs.percona.com/percona-xtrabackup/8.0/em/instant.html)

## References



* [The **xtrabackup** Option Reference](https://docs.percona.com/percona-xtrabackup/8.0/xtrabackup_bin/xbk_option_reference.html)


* [The xbcrypt binary](https://docs.percona.com/percona-xtrabackup/8.0/xbcrypt/xbcrypt.html)


* [The xbstream binary](https://docs.percona.com/percona-xtrabackup/8.0/xbstream/xbstream.html)


* [Frequently Asked Questions](https://docs.percona.com/percona-xtrabackup/8.0/faq.html)


* [Glossary](https://docs.percona.com/percona-xtrabackup/8.0/glossary.html)


* [Index of files created by Percona XtraBackup](https://docs.percona.com/percona-xtrabackup/8.0/xtrabackup-files.html)


* [Trademark Policy](https://docs.percona.com/percona-xtrabackup/8.0/trademark-policy.html)


* [Version checking](https://docs.percona.com/percona-xtrabackup/8.0/version-check.html)


