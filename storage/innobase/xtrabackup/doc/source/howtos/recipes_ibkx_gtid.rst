.. _recipes_ibkx_gtid:

================================================================================
How to create a new (or repair a broken) GTID based slave
================================================================================

|MySQL| 5.6 introduced the new Global Transaction ID (`GTID
<http://dev.mysql.com/doc/refman/5.6/en/replication-gtids-concepts.html>`_)
support in replication. |Percona XtraBackup| automatically
stores the ``GTID`` value in the :file:`xtrabackup_binlog_info` when doing the
backup of |MySQL| and |Percona Server| 5.7 with the ``GTID`` mode enabled. This
information can be used to create a new (or repair a broken) ``GTID`` based
replica.


STEP 1: Take a backup from any server on the replication environment, source or replica.
=========================================================================================

The following command takes a backup and saves it in the :file:`/data/backups/$TIMESTAMP` folder:

.. code-block:: bash

   $ innobackupex /data/backups/

In the destination folder, there will be a file with the name
:file:`xtrabackup_binlog_info`. This file contains both binary log coordinates and the ``GTID`` information.

.. code-block:: bash

   $ cat xtrabackup_binlog_info
   mysql-bin.000002    1232        c777888a-b6df-11e2-a604-080027635ef5:1-4

That information is also printed by innobackupex after taking the backup: 

.. code-block:: text

   innobackupex: MySQL binlog position: filename 'mysql-bin.000002', position 1232, GTID of the last change 'c777888a-b6df-11e2-a604-080027635ef5:1-4'

STEP 2: Prepare the backup
--------------------------------------------------------------------------------

The backup will be prepared with the following command:  

.. code-block:: console

   TheMaster$ innobackupex --apply-log /data/backups/$TIMESTAMP/

You need to select the path where your snapshot has been taken, for example
``/data/backups/2013-05-07_08-33-33``. If everything is ok you should get the
same OK message. Now, the transaction logs are applied to the data files, and new
ones are created: your data files are ready to be used by the MySQL server.

STEP 3: Move the backup to the destination server
--------------------------------------------------------------------------------

Use :command:`rsync` or :command:`scp` to copy the data to the destination
server. If you are synchronizing the data directly to the already running replica's data
directory it is advised to stop the |MySQL| server there.

.. code-block:: bash

   $ rsync -avprP -e ssh /path/to/backupdir/$TIMESTAMP NewSlave:/path/to/mysql/

After you copy the data over, make sure |MySQL| has proper permissions to access them.

.. code-block:: bash

   $ chown mysql:mysql /path/to/mysql/datadir

STEP 4: Configure and start replication
--------------------------------------------------------------------------------

Set the gtid_purged variable to the ``GTID`` from
:file:`xtrabackup_binlog_info`. Then, update the information about the
source node and, finally, start the replica.

.. code-block:: guess

   # Using the mysql shell
   NewSlave > SET SESSION wsrep_on = 0;
   NewSlave > RESET MASTER;
   NewSlave > SET SESSION wsrep_on = 1;
   NewSlave > SET GLOBAL gtid_purged='<gtid_string_found_in_xtrabackup_binlog_info>';
   NewSlave > CHANGE MASTER TO 
                MASTER_HOST="$masterip", 
                MASTER_USER="repl",
                MASTER_PASSWORD="$slavepass",
                MASTER_AUTO_POSITION = 1;
   NewSlave > START SLAVE;

.. note::

   The example above is applicable to |PXC|. The ``wsrep_on`` variable
   is set to `0` before resetting the source (``RESET MASTER``). The
   reason is that |PXC| will not allow resetting the source if
   ``wsrep_on=1``.

STEP 5: Check the replication status
--------------------------------------------------------------------------------

Following command will show the replica status:

.. code-block:: guess

   NewSlave > SHOW SLAVE STATUS\G
            [..]
            Slave_IO_Running: Yes
            Slave_SQL_Running: Yes
            [...]
            Retrieved_Gtid_Set: c777888a-b6df-11e2-a604-080027635ef5:5
            Executed_Gtid_Set: c777888a-b6df-11e2-a604-080027635ef5:1-5

We can see that the replica has retrieved a new transaction with number 5, so
transactions from 1 to 5 are already on this replica.

That's all, we have created a new replica in our ``GTID`` based replication environment.

.. |PXC| replace:: Percona XtraDB Cluster

