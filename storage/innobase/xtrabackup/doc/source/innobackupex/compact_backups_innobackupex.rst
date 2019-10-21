.. _compact_backups_ibk:

=================
 Compact Backups
=================

When doing the backup of |InnoDB| tables it's possible to omit the secondary
index pages. This will make the backups more compact and this way they will take
less space on disk. The downside of this is that the backup prepare process
takes longer as those secondary indexes need to be recreated. Difference in
backup size depends on the size of the secondary indexes.

For example full backup taken without and with the :option:`innobackupex
--compact` option: ::

  #backup size without --compact
  2.0G	2013-02-01_10-18-38

  #backup size taken with --compact option
  1.4G	2013-02-01_10-29-48

.. note::
   
   Compact backups are not supported for system table space, so in order to work
   correctly the ``innodb-file-per-table`` option should be enabled.

This feature was introduced in |Percona XtraBackup| 2.1.

Creating Compact Backups
========================

To make a compact backup innobackupex needs to be started with the
:option:`innobackupex --compact` option: ::

  $ innobackupex --compact /data/backups

This will create a timestamped directory in :file:`/data/backups`.

.. note:: 

   You can use the :option:`innobackupex --no-timestamp` option to override this
   behavior and the backup will be created in the given directory.

If you check at the :file:`xtrabackup_checkpoints` file in ``BASE-DIR``, you
should see something like::

  backup_type = full-backuped
  from_lsn = 0
  to_lsn = 2888984349
  last_lsn = 2888984349
  compact = 1

When :option:`innobackupex --compact` was not used ``compact`` value will be
``0``. This way, it's easy to check if the backup contains the secondary index
pages or not.

Preparing Compact Backups
=========================

Preparing the compact require rebuilding the indexes as well. In order to
prepare the backup a new option :option:`innobackupex --rebuild-indexes` should
be used with :option:`innobackupex --apply-log`: ::

  $ innobackupex --apply-log --rebuild-indexes /data/backups/2013-02-01_10-29-48

Output, beside the standard |innobackupex| output, should contain the
information about indexes being rebuilt, like: ::

  130201 10:40:20  InnoDB: Waiting for the background threads to start
  Rebuilding indexes for table sbtest/sbtest1 (space id: 10)
    Found index k_1
    Dropping 1 index(es).
    Rebuilding 1 index(es).
  Rebuilding indexes for table sbtest/sbtest2 (space id: 11)
    Found index k_1
    Found index c
    Found index k
    Found index c_2
    Dropping 4 index(es).
    Rebuilding 4 index(es).

Since |Percona XtraBackup| has no information when applying an incremental
backup to a compact full one, on whether there will be more incremental backups
applied to it later or not, rebuilding indexes needs to be explicitly requested
by a user whenever a full backup with some incremental backups merged is ready
to be restored. Rebuilding indexes unconditionally on every incremental backup
merge is not an option, since it is an expensive operation.

.. note::

   To process individual tables in parallel when rebuilding indexes,
   :option:`innobackupex --rebuild-threads` option can be used to specify the
   number of threads started by |Percona XtraBackup| when rebuilding secondary
   indexes on --apply-log --rebuild-indexes. Each thread rebuilds indexes for a
   single ``.ibd`` tablespace at a time.

Restoring Compact Backups
=========================

|innobackupex| has a :option:`innobackupex --copy-back` option, which performs
the restoration of a backup to the server's :term:`datadir` ::

  $ innobackupex --copy-back /path/to/BACKUP-DIR

It will copy all the data-related files back to the server's :term:`datadir`,
determined by the server's :file:`my.cnf` configuration file. You should check
the last line of the output for a success message::

  innobackupex: Finished copying back files.
  130201 11:08:13  innobackupex: completed OK!

Other Reading
=============

* `Feature preview: Compact backups in Percona XtraBackup <http://www.mysqlperformanceblog.com/2013/01/29/feature-preview-compact-backups-in-percona-xtrabackup/>`_

