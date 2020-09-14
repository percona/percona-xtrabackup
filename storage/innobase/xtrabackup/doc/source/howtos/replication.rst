.. _pxb.replication:
.. _replication_howto:

Setting up replication using |percona-xtrabackup|
================================================================================

:Procedure: :ref:`pxb.replication.setting-up`

You need a hot backup solution like |percona-xtrabackup| when the replication
source already has data. It is much more efficient to transfer data from the
replication source to replicas by using a backup rather than reading the binary
log of the replication source and running all recorded events on a
replica. After you make a backup on the replication source, you need to copy it
to the replica. The last step is to restore the backup on a replica.

|percona-xtrabackup| also facilitates setting up a replica based on the
data from another replica.

.. admonition:: Additional information

   There are two methods for configuring replication. You may either use the binary
   log or |abbr.gtid|. |percona-xtrabackup| supports both methods.

   .. seealso::

      |mysql| documentation:
         - `Setting up replication based on binary logging
	   <https://dev.mysql.com/doc/refman/8.0/en/replication-configuration.html>`_
	 - `Setting up transaction based replication
	   <https://dev.mysql.com/doc/refman/8.0/en/replication-gtids.html>`_

When replication is configured to use `GTID mode`_, the replication source,
which is the target of all database updates, automatically gefnerates global
transaction identifiers (|abbr.gtid|). Using |abbr.gtid|, replicas can determine
which transactions have been applied - the binary log in this replication
configuration is not needed.

.. warning::

   Do not mix |percona-server| and |mysql| server in your replication
   setup. This may produce an incorrect |file.xtrabackup-slave-info| file when
   adding a new replica.

|percona-xtrabackup| saves both binary log coordinates (effective binary log
file and the position in it) and the |abbr.gtid| in the |file.binlog-info| file
when making a backup of a |mysql| or |percona-server| 8.0. Thus,
|file.binlog-info| is sufficient to restore the backup for binary log based
replication as well as |abbr.gtid| based replication.

|percona-xtrabackup| makes a backup of the data on the replication source and
records the coordinates of the binary log. As soon as the replica is ready, its
binary log is used for its usual purpose: keeping record of events that change
the database.

.. _pxb.replication.setting-up.summary:

.. rubric:: Summary

In outline, to set up replication using |percona-xtrabackup|, complete the
following steps:

#. :ref:`pxb.replication.setting-up/replication-source.backing-up`
#. :ref:`pxb.replication.setting-up/backup.preparing`
#. :ref:`pxb.replication.setting-up/backup.replica.moving`
#. :ref:`pxb.replication.setting-up/replication.starting`

Repeat this procedure for each new replica. You may reuse the same
backup. However, each replica in your configuration must have a distinct
`server_id`.

.. rubric:: Creating a replica based on another replica

You may use a replica as the source of backup instead of the replication source. The
replica that you use as the source of backup must be a replica
consistent with the replication source. Otherwise, the procedure is identical to
:ref:`the case when you create a replica based on the primary server
<pxb.replication.setting-up.summary>`.

A good tool to check consistency is `pt-table-checksum
<http://www.percona.com/doc/percona-toolkit/2.2/pt-table-checksum.html>`_.

.. rubric:: Parameters

.. list-table::
   :header-rows: 1
   :widths: 25 75
		
   * - Parameter
     - Use for
   * - :option:`--slave-info`
     - Backing up a replica. This option saves information about the replication
       source (binary log coordinates) to |file.xtrabackup-slave-info| as a
       |sql.change-master| statement.
   * - :option:`--safe-slave-backup`
     - Stopping the replication SQL thread; |xtrabackup| starts the backup after
       ``Slave_open_temp_tables`` in |sql.show-status| is zero. This option is
       recommended when making a backup from another replica.


.. Replacements ================================================================

.. include:: ../_res/links.txt
.. include:: ../_res/replace/abbr.txt
.. include:: ../_res/replace/file.txt
.. include:: ../_res/replace/proper.txt
.. include:: ../_res/replace/sql.txt
