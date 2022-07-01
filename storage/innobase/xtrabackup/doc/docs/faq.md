# Frequently Asked Questions

## Do I need an InnoDB Hot Backup license to use Percona XtraBackup?

No. Although `innobackupex` is derived from the same GPL and open-source
wrapper script that InnoDB Hot Backup uses, it does not execute `ibbackup`,
and the `xtrabackup` binary does not execute or link to `ibbackup`. You
can use *Percona  XtraBackup* without any license; it is completely separate
from InnoDB Hot Backup.

## What’s the difference between innobackupex and innobackup?

The innobackupex binary is a patched version of the *Oracle* innobackup script (renamed mysqlbackup). They are similar, and familiarity with innobackup might be helpful.

Besides the available options for specific features of *innobackupex*, the main differences are:

* Prints to `STDERR` instead of `STDOUT` which enables the `innobackupex --stream` option

* Detects the configuration file - `my.cnf` - is automatically (or set with `innobackupex --defaults-file`) instead of requiring the configuration file as the the first argument

* Defaults to *xtrabackup* as binary to use in the `innobackupex --ibbackup`

See [The innobackupex Option Reference](innobackupex/innobackupex_option_reference.md) for more details.

## Which Web-based backup tools are based on Percona XtraBackup?

[Zmanda Recovery Manager](http://www.zmanda.com/zrm-mysql-enterprise.html) is
a commercial tool that uses *Percona  XtraBackup* for Non-Blocking Backups:

 *“ZRM provides support for non-blocking backups of MySQL using *Percona XtraBackup*. ZRM with *Percona  XtraBackup* provides resource utilization management by providing throttling based on the number of IO operations per second. *Percona  XtraBackup* based backups also allow for table-level recovery even though the backup was done at the database level. This operation requires the recovery database server to be *Percona Server for MySQL* with XtraDB.”*

## *xtrabackup* binary fails with a floating point exception

In most of the cases this is due to not having installed the required libraries (and version) by *xtrabackup*. Installing the *GCC* suite with the supporting libraries and recompiling *xtrabackup* will solve the issue. See [Compiling and Installing from Source Code](installation/compiling_xtrabackup.md) for instructions on the procedure.

## How does xtrabackup handle the `ibdata/ib_log` files on restore if they are not in the MySQL datadir?

If the `ibdata` and `ib_log` files are located in different
directories outside of the datadir, you move them to their proper place after the logs have been applied.

## Backup fails with Error 24: ‘Too many open files’

This error usually occurs when the database being backed up contains large amount of files and *Percona  XtraBackup* can’t open all of them to create a successful backup. In order to avoid this error the operating system should be configured appropriately so that *Percona  XtraBackup* can open all its files. On Linux, this can be done with the `ulimit` command for specific backup session or by editing the /etc/security/limits.conf to change it globally

!!! note

    The maximum possible value that can be set up is `1048576` which is a hard-coded constant in the Linux kernel.

## How to deal with skipping of redo logs for DDL operations?

To prevent creating corrupted backups when running DDL operations,
Percona XtraBackup aborts if it detects that redo logging is disabled.
In this case, the following error is printed:

```default
[FATAL] InnoDB: An optimized (without redo logging) DDL operation has been performed. All modified pages may not have been flushed to the disk yet.
Percona XtraBackup will not be able to take a consistent backup. Retry the backup operation.
```

!!! note

    Redo logging is disabled during a [sorted index build](https://dev.mysql.com/doc/refman/5.7/en/sorted-index-builds.html)

To avoid this error,
Percona XtraBackup can use metadata locks on tables while they are copied:

* To block all DDL operations, use the `xtrabackup --lock-ddl` option
that issues `LOCK TABLES FOR BACKUP`.

* If `LOCK TABLES FOR BACKUP` is not supported, you can block DDL for each
table before XtraBackup starts to copy it and until the backup is completed
using the `xtrabackup --lock-ddl-per-table` option.
