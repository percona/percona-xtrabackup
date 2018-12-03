.. _compact_backups:

================================================================================
 Compact Backups
================================================================================

When doing the backup of |InnoDB| tables it's possible to omit the secondary
index pages. This will make the backups more compact and this way they will take
less space on disk. The downside of this is that the backup prepare process
takes longer as those secondary indexes need to be recreated. Difference in
backup size depends on the size of the secondary indexes.

For example full backup taken without and with the :option:`xtrabackup
--compact` option: ::

  #backup size without --compact 
  2.0G	xb_backup

  #backup size taken with --compact option
  1.4G	xb_compact_backup

.. note::
   
   Compact backups are not supported for system table space, so in order to work
   correctly the ``innodb-file-per-table`` option should be enabled.

This feature was introduced in |Percona XtraBackup| 2.1.

Creating Compact Backups
================================================================================

To make a compact backup innobackupex needs to be started with the
:option:`xtrabackup --compact` option: ::

  $ xtrabackup --backup --compact --target-dir=/data/backups

This will create a compact backup in the :file:`/data/backups`.

If you check at the :file:`xtrabackup-checkpoints` file in the ``target-dir``
folder, you should see something like::

  backup_type = full-backuped
  from_lsn = 0
  to_lsn = 2888984349
  last_lsn = 2888984349
  compact = 1

When :option:`xtrabackup --compact` wasn't used ``compact`` value will be
``0``. This way it's easy to check if the backup contains the secondary index
pages or not.

Preparing Compact Backups
================================================================================

Preparing the compact require rebuilding the indexes as well. In order to
prepare the backup use :option:`xtrabackup --prepare`: ::

  $ xtrabackup --prepare /data/backups/

Output, beside the standard |innobackupex| output, should contain the
information about indexes being rebuilt, like: ::

  [01] Checking if there are indexes to rebuild in table sakila/city (space id: 9)
  [01]   Found index idx_fk_country_id
  [01]   Rebuilding 1 index(es).
  [01] Checking if there are indexes to rebuild in table sakila/country (space id: 10)
  [01] Checking if there are indexes to rebuild in table sakila/customer (space id: 11)
  [01]   Found index idx_fk_store_id
  [01]   Found index idx_fk_address_id
  [01]   Found index idx_last_name
  [01]   Rebuilding 3 index(es).

Since |Percona XtraBackup| has no information when applying an incremental
backup to a compact full one, on whether there will be more incremental backups
applied to it later or not, rebuilding indexes needs to be explicitly requested
by a user whenever a full backup with some incremental backups merged is ready
to be restored. Rebuilding indexes unconditionally on every incremental backup
merge is not an option, since it is an expensive operation.

Restoring Compact Backups
=========================

The |xtrabackup| binary does not have any functionality for restoring a
backup. That is up to the user to do. You might use :program:`rsync` or
:program:`cp` to restore the files. You should check that the restored files
have the correct ownership and permissions.

Other Reading
=============

* `Feature preview: Compact backups in Percona XtraBackup <http://www.mysqlperformanceblog.com/2013/01/29/feature-preview-compact-backups-in-percona-xtrabackup/>`_

