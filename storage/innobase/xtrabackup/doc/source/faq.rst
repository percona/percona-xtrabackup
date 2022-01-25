================================================================================
Frequently Asked Questions
================================================================================

Does *Percona XtraBackup* 8.0 support making backups of databases in versions prior to 8.0?
====================================================================================================

*Percona XtraBackup* 8.0 does not support making backups of databases
created in versions prior to 8.0 of |MySQL|, |Percona Server| or
|Percona XtraDB Cluster|. The changes that |MySQL| 8.0 introduced
in *data dictionaries*, *redo log* and *undo log* are incompatible
with the previous versions. It is currently impossible for *Percona XtraBackup* 8.0 to also support versions prior to 8.0.

Due to changes in MySQL 8.0.20 released by Oracle at the end of April 2020,
*Percona XtraBackup* 8.0, up to version 8.0.11, is not compatible with MySQL version 8.0.20 or
higher, or Percona products that are based on it: Percona Server for MySQL and
Percona XtraDB Cluster.

For more information, see `Percona XtraBackup 8.x and MySQL 8.0.20
<https://www.percona.com/blog/2020/04/28/percona-xtrabackup-8-x-and-mysql-8-0-20/>`_

.. _pxb.faq.innobackupex.8-0:

Why will ``innobackupex`` not run in |Percona Xtrabackup| 8.0?
================================================================================

:program:`innobackupex` has been removed from |Percona XtraBackup|
|version| in favor of :program:`xtrabackup`.

What's the difference between :program:`innobackupex` and :program:`xtrabackup`?
================================================================================

See :ref:`pxb.faq.innobackupex.8-0`

Are you aware of any web-based backup management tools (commercial or not) built around |Percona XtraBackup|?
========================================================================================================================

`Zmanda Recovery Manager <http://www.zmanda.com/zrm-mysql-enterprise.html>`_ is
a commercial tool that uses |Percona XtraBackup| for Non-Blocking Backups:

 *"ZRM provides support for non-blocking backups of MySQL using |Percona
 XtraBackup|. ZRM with |Percona XtraBackup| provides resource utilization
 management by providing throttling based on the number of IO operations per
 second. |Percona XtraBackup| based backups also allow for table level recovery
 even though the backup was done at the database level (needs the recovery
 database server to be |Percona Server| with XtraDB)."*

*Percona XtraBackup* binary fails with a floating point exception
================================================================================

In most of the cases this is due to not having install the required libraries
(and version) by *Percona XtraBackup*. Installing the *GCC* suite with the supporting
libraries and recompiling *Percona XtraBackup* will solve the issue. See
:doc:`installation/compiling_xtrabackup` for instructions on the procedure.

How xtrabackup handles the ibdata/ib_log files on restore if they aren't in mysql datadir?
====================================================================================================

In case the :file:`ibdata` and :file:`ib_log` files are located in different
directories outside of the datadir, you will have to put them in their proper
place after the logs have been applied.

Backup fails with Error 24: 'Too many open files'
=================================================

This usually happens when database being backed up contains large amount of
files and |Percona XtraBackup| can't open all of them to create a successful
backup. In order to avoid this error the operating system should be configured
appropriately so that |Percona XtraBackup| can open all its files. On Linux,
this can be done with the ``ulimit`` command for specific backup session or by
editing the :file:`/etc/security/limits.conf` to change it globally (**NOTE**:
the maximum possible value that can be set up is ``1048576`` which is a
hard-coded constant in the Linux kernel).

How to deal with skipping of redo logs for DDL operations?
==========================================================

To prevent creating corrupted backups when running DDL operations,
Percona XtraBackup aborts if it detects that redo logging is disabled.
In this case, the following error is printed::

 [FATAL] InnoDB: An optimized (without redo logging) DDL operation has been performed. All modified pages may not have been flushed to the disk yet.
 Percona XtraBackup will not be able to take a consistent backup. Retry the backup operation.

.. note:: Redo logging is disabled during a `sorted index build
   <https://dev.mysql.com/doc/refman/5.7/en/sorted-index-builds.html>`_

To avoid this error,
Percona XtraBackup can use metadata locks on tables while they are copied:

* To block all DDL operations, use the :option:`--lock-ddl` option
  that issues ``LOCK TABLES FOR BACKUP``.

* If ``LOCK TABLES FOR BACKUP`` is not supported,
  you can block DDL for each table
  before XtraBackup starts to copy it
  and until the backup is completed
  using the :option:`--lock-ddl-per-table` option.
  
  .. note::
  
      As of |PXB| 8.0.15, the `--lock-ddl-per-table` option is deprecated.
      Use the `--lock-ddl` option. 

.. |version| replace:: '8.0'
.. seealso::

    :ref:`lock_redesign`
