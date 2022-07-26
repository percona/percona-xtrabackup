
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
a feature comparison, please see [About Percona XtraBackup](intro.md).

## Introduction

* [About Percona XtraBackup](intro.md)

* [How *Percona XtraBackup* Works](how_xtrabackup_works.md)

## Installation

* [Installing *Percona XtraBackup* on *Debian* and *Ubuntu*](installation/apt_repo.md)

* [Installing *Percona XtraBackup* on *Red Hat Enterprise Linux* and *CentOS*](installation/yum_repo.md)

* [Installing *Percona XtraBackup* from a Binary Tarball](installation/binary-tarball.md)

* [Compiling and Installing from Source Code](installation/compiling_xtrabackup.md)

## Run in Docker

* [Running Percona XtraBackup in a Docker container](installation/docker.md)

## How Percona XtraBackup works

* [Implementation Details](xtrabackup_bin/implementation_details.md)

* [Connection and Privileges Needed](using_xtrabackup/privileges.md)

* [Configuring xtrabackup](using_xtrabackup/configuring.md)

* [Server Version and Backup Version Comparison](using_xtrabackup/comparison.md)

* [*xtrabackup* Exit Codes](xtrabackup_bin/xtrabackup_exit_codes.md)

## Backup Scenarios

* [The Backup Cycle - Full Backups](backup_scenarios/full_backup.md)

* [Incremental Backup](backup_scenarios/incremental_backup.md)

* [Compressed Backup](backup_scenarios/compressed_backup.md)

* [Partial Backups](xtrabackup_bin/partial_backups.md)

## User’s Manual

* [*Percona XtraBackup* User Manual](manual.md)

## Advanced Features

* [Throttling Backups](advanced/throttling_backups.md)

* [Encrypted InnoDB tablespace backups](advanced/encrypted_innodb_tablespace_backups.md)

* [Encrypting Backups](xtrabackup_bin/backup.encrypting.md)

* [LRU dump backup](xtrabackup_bin/lru_dump.md)

* [Point-In-Time recovery](xtrabackup_bin/point-in-time-recovery.md)

* [Working with Binary Logs](xtrabackup_bin/working_with_binary_logs.md)

* [Improved Log statements](advanced/log_enhancements.md)

## Security

* [Working with SELinux](security/pxb-selinux.md)

* [Working with AppArmor](security/pxb-apparmor.md)

## Auxiliary guides

* [Enabling the server to communicate via TCP/IP](howtos/enabling_tcp.md)

* [Installing and configuring an SSH server](howtos/ssh_server.md)

* [Analyzing table statistics](xtrabackup_bin/analyzing_table_statistics.md)

* [`FLUSH TABLES WITH READ LOCK` option](xtrabackup_bin/flush-tables-with-read-lock.md)

* [`lock-ddl-per-table` option improvements](advanced/locks.md)

* [Incremental backup using page tracking](advanced/page_tracking.md)

## xbcloud Binary

* [The xbcloud Binary](xbcloud/xbcloud.md)

* [Using the xbcloud Binary with Swift](xbcloud/xbcloud_swift.md)

* [Using xbcloud Binary with Amazon S3](xbcloud/xbcloud_s3.md)

* [Using the xbcloud Binary with MinIO](xbcloud/xbcloud_minio.md)

* [Using the xbcloud with Google Cloud Storage](xbcloud/xbcloud_gcs.md)

* [Exponential Backoff](xbcloud/xbcloud_exbackoff.md)

* [Using the xbcloud binary with Microsoft Azure Cloud Storage](xbcloud/xbcloud_azure.md)

## Tutorials, Recipes, How-tos

* [Recipes for xtrabackup](how-tos.md#recipes-xbk)

* [How-Tos](how-tos.md#howtos)

## Release notes

* [*Percona XtraBackup* 8.0 Release Notes](release-notes.md)

## Error message descriptions

* [Error Message: Found tables with row version due to INSTANT ADD/DROP columns](em/instant.md)

## References

* [The **xtrabackup** Option Reference](xtrabackup_bin/xbk_option_reference.md)

* [The xbcrypt binary](xbcrypt/xbcrypt.md)

* [The xbstream binary](xbstream/xbstream.md)

* [Frequently Asked Questions](faq.md)

* [Glossary](glossary.md)

* [Index of files created by Percona XtraBackup](xtrabackup-files.md)

* [Trademark Policy](trademark-policy.md)

* [Version checking](version-check.md)

