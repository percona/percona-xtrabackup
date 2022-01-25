.. _pxb.xtrabackup.point-in-time-recovery:

================================================================================
Point-In-Time recovery
================================================================================

Recovering up to particular moment in database's history can be done with
*Percona XtraBackup* and the binary logs of the server.

Note that the binary log contains the operations that modified the database from
a point in the past. You need a full :term:`datadir` as a base, and then you can
apply a series of operations from the binary log to make the data match what it
was at the point in time you want.

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/path/to/backup
   $ xtrabackup --prepare --target-dir=/path/to/backup

For more details on these procedures, see :ref:`creating_a_backup` and :ref:`preparing_a_backup`.

Now, suppose that some time has passed, and you want to restore the database to a
certain point in the past, having in mind that there is the constraint of the
point where the snapshot was taken.

To find out what is the situation of binary logging in the server, execute the
following queries: ::

  mysql> SHOW BINARY LOGS;
  +------------------+-----------+
  | Log_name         | File_size |
  +------------------+-----------+
  | mysql-bin.000001 |       126 |
  | mysql-bin.000002 |      1306 |
  | mysql-bin.000003 |       126 |
  | mysql-bin.000004 |       497 |
  +------------------+-----------+

and ::

  mysql> SHOW MASTER STATUS;
  +------------------+----------+--------------+------------------+
  | File             | Position | Binlog_Do_DB | Binlog_Ignore_DB |
  +------------------+----------+--------------+------------------+
  | mysql-bin.000004 |      497 |              |                  |
  +------------------+----------+--------------+------------------+

The first query will tell you which files contain the binary log and the second
one which file is currently being used to record changes, and the current
position within it. Those files are stored usually in the :term:`datadir`
(unless other location is specified when the server is started with the
``--log-bin=`` option).

To find out the position of the snapshot taken, see the
:file:`xtrabackup_binlog_info` at the backup's directory: ::

  $ cat /path/to/backup/xtrabackup_binlog_info
  mysql-bin.000003	57

This will tell you which file was used at moment of the backup for the binary
log and its position. That position will be the effective one when you restore
the backup: ::

  $ xtrabackup --copy-back --target-dir=/path/to/backup

As the restoration will not affect the binary log files (you may need to adjust
file permissions, see :ref:`restoring_a_backup`), the next step is
extracting the queries from the binary log with :command:`mysqlbinlog` starting
from the position of the snapshot and redirecting it to a file ::

  $ mysqlbinlog /path/to/datadir/mysql-bin.000003 /path/to/datadir/mysql-bin.000004 \ 
      --start-position=57 > mybinlog.sql

Note that if you have multiple files for the binary log, as in the example, you
have to extract the queries with one process, as shown above.

Inspect the file with the queries to determine which position or date
corresponds to the point-in-time wanted. Once determined, pipe it to the
server. Assuming the point is ``11-12-25 01:00:00``::

  $ mysqlbinlog /path/to/datadir/mysql-bin.000003 /path/to/datadir/mysql-bin.000004 \ 
      --start-position=57 --stop-datetime="11-12-25 01:00:00" | mysql -u root -p

and the database will be rolled forward up to that Point-In-Time.
