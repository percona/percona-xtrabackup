# Percona XtraBackup - Documentation

*Percona XtraBackup* is an open-source hot backup utility for *MySQL* - based
servers that does not lock your database during the backup. It can back up data
from *InnoDB*, XtraDB, and *MyISAM* tables on *MySQL* 5.1 , 5.5, 5.6 and 5.7 servers, as well as Percona Server with XtraDB.

!!! note

    Support for InnoDB 5.1 builtin has been removed in *Percona XtraBackup* 2.1

For a high-level overview of many of its advanced features, including a feature
comparison, please see [About Percona XtraBackup](intro.md).

Whether it is a 24x7 highly loaded server or a low-transaction-volume
environment, *Percona XtraBackup* is designed to make backups a seamless
procedure without disrupting the performance of the server in a production
environment. [Commercial support contracts are available](https://www.percona.com/mysql-support/).

!!! important

    Percona XtraBackup 2.4 does not support making backups of databases created in *MySQL 8.0*, *Percona Server for MySQL 8.0*, or *Percona XtraDB Cluster 8.0*.
    
    Use `Percona XtraBackup 8.0 <https://www.percona.com/downloads/Percona-XtraBackup-LATEST/#>`__ for making backups of databases in *MySQL 8.0*, *Percona Server for MySQL 8.0*, and *Percona XtraDB Cluster 8.0*.

## Introduction

* [About Percona XtraBackup](intro.md)

* [How *Percona XtraBackup* Works](how_xtrabackup_works.md)

## Installation

* [Installing *Percona XtraBackup* on *Debian* and *Ubuntu*](installation/apt_repo.md)

* [Installing *Percona XtraBackup* on Red Hat Enterprise Linux and CentOS](installation/yum_repo.md)

* [Installing *Percona XtraBackup* from a Binary Tarball](installation/binary-tarball.md)

* [Compiling and Installing from Source Code](installation/compiling_xtrabackup.md)

## Run in Docker

* [Running Percona XtraBackup in a Docker container](installation/docker.md)

## Prerequisites

* [Connection and Privileges Needed](using_xtrabackup/privileges.md)

* [Configuring xtrabackup](using_xtrabackup/configuring.md)

## Backup Scenarios

* [The Backup Cycle - Full Backups](backup_scenarios/full_backup.md)

* [Incremental Backup](backup_scenarios/incremental_backup.md)

* [Compressed Backup](backup_scenarios/compressed_backup.md)

* [Encrypted Backup](backup_scenarios/encrypted_backup.md)

## User’s Manual

* [*Percona XtraBackup* User Manual](manual.md)

## Advanced Features

* [Throttling Backups](advanced/throttling_backups.md)

* [Lockless binary log information](advanced/lockless_bin-log.md)

* [Encrypted InnoDB Tablespace Backups](advanced/encrypted_innodb_tablespace_backups.md)

* [`lock-ddl-per-table` Option Improvements](advanced/locks.md)

## Tutorials, Recipes, How-tos

* [Recipes for xtrabackup](how-tos.md#recipes-xbk)

* [Recipes for innobackupex](how-tos.md#recipes-ibk)

* [How-Tos](how-tos.md#howtos)

* [Auxiliary Guides](how-tos.md#aux-guides)

## References

* [*Percona XtraBackup* 2.4 Release Notes](release-notes.md)

* [The xtrabackup Option Reference](xtrabackup_bin/xbk_option_reference.md)

* [The innobackupex Option Reference](innobackupex/innobackupex_option_reference.md)

* [The xbcloud Binary](xbcloud/xbcloud.md)

* [Exponential Backoff](xbcloud/xbcloud_exbackoff.md)

* [Using the xbcloud binary with Microsoft Azure Cloud Storage](xbcloud/xbcloud_azure.md)

* [The xbcrypt binary](xbcrypt/xbcrypt.md)

* [The xbstream binary](xbstream/xbstream.md)

* [Known issues and limitations](known_issues.md)

* [Frequently Asked Questions](faq.md)

* [Glossary](glossary.md)

* [Index of files created by Percona XtraBackup](xtrabackup-files.md)

* [Trademark Policy](trademark-policy.md)

* [Version checking](version-check.md)
