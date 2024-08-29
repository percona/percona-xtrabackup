.. _working_with_binlogs:

Working with Binary Logs
========================

The ``xtrabackup`` binary integrates with the ``log_status table``. This integration enables ``xtrabackup`` to print out the backup's corresponding binary log position, so that you can use this binary log position to provision a new replica or perform point-in-time recovery.

Finding the Binary Log Position
--------------------------------

You can find the binary log position corresponding to a backup after the backup has been taken. If your backup is from a server with binary logging enabled, ``xtrabackup`` creates a file named ``xtrabackup_binlog_info`` in the target directory. This file contains the binary log file name and position of the exact point when the backup was taken.

The output is similar to the following during the backup stage:

.. sourcecode:: text

    210715 14:14:59 Backup created in directory '/backup/'
    MySQL binlog position: filename 'binlog.000002', position '156'
    . . .
    210715 14:15:00 completed OK!
 
.. note::

  As of Percona XtraBackup 8.0.26-18.0, xtrabackup no longer creates the ``xtrabackup_binlog_pos_innodb`` file. This change is because MySQL and Percona Server no longer update the binary log information on global transaction system section of ``ibdata``. You should rely on ``xtrabackup_binlog_info`` regardless of the storage engine in use.

Point-In-Time Recovery
-----------------------

To perform a point-in-time recovery from an ``xtrabackup`` backup, you should prepare and restore the backup, and then replay binary logs from the point shown in the :file:`xtrabackup_binlog_info` file. 

A more detailed procedure is found :ref:`here <pxb.xtrabackup.point-in-time-recovery>`.


Setting Up a New Replication Replica
-------------------------------------

To set up a new replica, you should prepare the backup, and restore it to the data directory of your new replication replica. If you are using version 8.0.22 or earlier, in your ``CHANGE MASTER TO`` command, use the binary log filename and position shown in the :file:`xtrabackup_binlog_info` file to start replication. 

If you are using 8.0.23 or later, use the `CHANGE_REPLICATION_SOURCE_TO and the appropriate options <https://dev.mysql.com/doc/refman/8.0/en/change-replication-source-to.html>`__. ``CHANGE_MASTER_TO`` is deprecated. 

A more detailed procedure is found in  :doc:`../howtos/setting_up_replication`.
