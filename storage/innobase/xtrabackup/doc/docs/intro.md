# About Percona XtraBackup

*Percona XtraBackup* is the world’s only open-source, free *MySQL* hot backup
software that performs non-blocking backups for *InnoDB* and *XtraDB*
databases. With *Percona XtraBackup*, you can achieve the following benefits:


* Backups that complete quickly and reliably


* Uninterrupted transaction processing during backups


* Savings on disk space and network bandwidth


* Automatic backup verification


* Higher uptime due to faster restore time

*Percona XtraBackup* makes *MySQL* hot backups for all versions of Percona
Server for MySQL, and *MySQL*. It performs streaming, compressed, and incremental *MySQL*
backups.

**_Important_**

> **Percona XtraBackup** 2.4 supports *MySQL* and *Percona Server for 
> MySQL* 
5.6 and 5.7 databases. Due to changes in the MySQL redo log and data 
dictionary formats, the **Percona XtraBackup** 8.0.x versions are only 
compatible with *MySQL* 8.0.x, *Percona Server for MySQL* 8.0.x, and 
compatible versions.

Percona’s enterprise-grade commercial [MySQL Support](http://www.percona.com/mysql-support/) contracts include support for *Percona
XtraBackup*. We recommend support for critical production deployments. Percona XtraDB Backup supports encryption.

### Supported storage engines

Percona XtraBackup works with MySQL and Percona Server. It supports
completely non-blocking backups of InnoDB, XtraDB, and MyRocks storage
engines. Fast incremental backups are supported for Percona Server with the XtraDB changed page tracking enabled.

In addition, it can back up the following storage engines by briefly
pausing writes at the end of the backup: MyISAM and Merge, including partitioned tables, triggers, and database
options. The InnoDB tables are still locked while copying non-InnoDB data.

**_Version Updates_**

Version 8.0.6 and later supports the MyRocks storage engine. 
An incremental backup on the MyRocks storage engine does not 
determine if an earlier full or incremental backup 
contains the same files. **Percona XtraBackup** copies all 
MyRocks files each time it takes a backup.
**Percona 
XtraBackup** does not support the TokuDB storage engine. 

_See Also_ [Percona TokuBackup](https://docs.percona.com/percona-server/latest/tokudb/toku_backup.html) 

## What are the features of Percona XtraBackup?

Here is a short list of the Percona XtraBackup features. See the documentation
for more.


* Create hot InnoDB backups without pausing your database


* Make incremental backups of MySQL


* Stream compressed MySQL backups to another server


* Move tables between MySQL servers on-line


* Create new MySQL replication replicas easily


* Backup MySQL without adding load to the server


* Percona XtraBackup performs throttling based on the number of IO operations per second


* Percona XtraBackup skips secondary index pages and recreates them when a compact backup is prepared


* Percona XtraBackup can export individual tables even from a full backup, regardless of the InnoDB version


* Backup locks is a lightweight alternative to `FLUSH TABLES WITH READ LOCK` available in Percona Server. Percona XtraBackup uses them automatically to copy non-InnoDB data to avoid blocking DML queries that modify InnoDB tables.

**_See also_**
[How Percona XtraBackup works](https://docs.percona.com/percona-xtrabackup/8.0/how_xtrabackup_works.html#how-xtrabackup-works)

### Additional information

The *InnoDB* tables are locked while copying non-*InnoDB* data.
