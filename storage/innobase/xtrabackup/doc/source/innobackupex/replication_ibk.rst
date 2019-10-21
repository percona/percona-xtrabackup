============================================
 Taking Backups in Replication Environments
============================================

There are options specific to back up from a replication slave.

:option:`innobackupex --slave-info`
================================================================================

This option is useful when backing up a replication slave server. It prints the
binary log position and name of the master server. It also writes this
information to the :file:`xtrabackup_slave_info` file as a ``CHANGE MASTER``
statement.

This is useful for setting up a new slave for this master can be set up by
starting a slave server on this backup and issuing the statement saved in the
:file:`xtrabackup_slave_info` file. More details of this procedure can be found
in :ref:`replication_howto`.

:option:`innobackupex --safe-slave-backup`
================================================================================

In order to assure a consistent replication state, this option stops the slave
SQL thread and wait to start backing up until ``Slave_open_temp_tables`` in
``SHOW STATUS`` is zero. If there are no open temporary tables, the backup will
take place, otherwise the SQL thread will be started and stopped until there are
no open temporary tables. The backup will fail if ``Slave_open_temp_tables``
does not become zero after :option:`innobackupex --safe-slave-backup-timeout`
seconds (defaults to 300 seconds). The slave SQL thread will be restarted when
the backup finishes.

Using this option is always recommended when taking backups from a slave server.

.. warning:: 

   Make sure your slave is a true replica of the master before using it as a
   source for backup. A good tool to validate a slave is `pt-table-checksum
   <http://www.percona.com/doc/percona-toolkit/2.2/pt-table-checksum.html>`_.
