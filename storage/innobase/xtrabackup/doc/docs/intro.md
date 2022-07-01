# About Percona XtraBackup

*Percona XtraBackup* is the world’s only open-source, free *MySQL* hot backup
software that performs non-blocking backups for *InnoDB* and XtraDB
databases. With *Percona XtraBackup*, you can achieve the following benefits:

* Backups that complete quickly and reliably

* Uninterrupted transaction processing during backups

* Savings on disk space and network bandwidth

* Automatic backup verification

* Higher uptime due to faster restore time

See the compatibility matrix in [Percona Software and Platform Lifecycle](https://www.percona.com/services/policies/percona-software-platform-lifecycle)

to find out which versions of MySQL, MariaDB, and Percona Server for MySQL are
supported by Percona XtraBackup and supports encryption with any kind of backups.

Non-blocking backups of InnoDB, Percona XtraDB Cluster, and *HailDB* storage engines are supported.
In addition, Percona XtraBackup can back up the following storage engines by briefly pausing writes
at the end of the backup: MyISAM, `Merge <.MRG>`, and `Archive <.ARM>`, including partitioned tables,
triggers, and database options. InnoDB tables are still locked while copying non-InnoDB data.
Fast incremental backups are supported for Percona Server with Percona XtraDB Cluster changed page
tracking enabled.

!!! important

    Percona XtraBackup 2.4 only supports Percona XtraDB Cluster 5.7. Percona XtraBackup 2.4 does not support the MyRocks storage engine or TokuDB storage engine. *Percona XtraBackup* is not compatible with MariaDB 10.3 and later.

Percona’s enterprise-grade commercial [MySQL Support](http://www.percona.com/mysql-support/) contracts include support for
*Percona XtraBackup*. We recommend support for critical production deployments.

## What are the features of Percona XtraBackup?

Here is a short list of *Percona XtraBackup* features. See the documentation
for more.

* Create hot InnoDB backups without pausing your database

* Make incremental backups of MySQL

* Stream compressed MySQL backups to another server

* Move tables between MySQL servers on-line

* Create new MySQL replication replicas easily

* Backup MySQL without adding load to the server

* Backup locks are a lightweight alternative to `FLUSH TABLES WITH READ LOCK` available in Percona Server 5.6+. Percona XtraBackup uses them automatically to copy non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.

* Percona XtraBackup performs throttling based on the number of IO operations per second.

* Percona XtraBackup skips secondary index pages and recreates them when a compact backup is prepared.

* Percona XtraBackup can export individual tables even from a full backup, regardless of the InnoDB version.

* Tables exported with Percona XtraBackup can be imported into Percona Server 5.1, 5.5 or 5.6+, or MySQL 5.6+.
