.. _recipes_ibkx_gtid:

================================================================================
How to create a new (or repair a broken) GTID based slave
================================================================================

|MySQL| 5.6 introduced the new Global Transaction ID (`GTID
<http://dev.mysql.com/doc/refman/5.6/en/replication-gtids-concepts.html>`_)
support in replication. |Percona XtraBackup| from 2.1.0 version, automatically
stores the ``GTID`` value in the :file:`xtrabackup_binlog_info` when doing the
backup of |MySQL| and |Percona Server| 5.6 with the ``GTID`` mode enabled. This
information can be used to create a new (or repair a broken) ``GTID`` based
slave.


STEP 1: Take a backup from any server on the replication environment, master or slave
-------------------------------------------------------------------------------------

Following command will take a backup to the /data/backups/$TIMESTAMP folder: :: 

 $ innobackupex /data/backups/

In the destination folder there will be a file with the name
:file:`xtrabackup_binlog_info`. This file will contain both, binary log
coordinates and ``GTID`` information. ::

 $ cat xtrabackup_binlog_info
 mysql-bin.000002    1232        c777888a-b6df-11e2-a604-080027635ef5:1-4

That information is also printed by innobackupex after backup is taken: ::

 innobackupex: MySQL binlog position: filename 'mysql-bin.000002', position 1232, GTID of the last change 'c777888a-b6df-11e2-a604-080027635ef5:1-4'

STEP 2: Prepare the backup
--------------------------------------------------------------------------------

Back will be prepared with the following command:  

.. code-block:: console

   TheMaster$ innobackupex --apply-log /data/backups/$TIMESTAMP/

You need to select path where your snapshot has been taken, for example
``/data/backups/2013-05-07_08-33-33``. If everything is ok you should get the
same OK message. Now the transaction logs are applied to the data files, and new
ones are created: your data files are ready to be used by the MySQL server.

STEP 3: Move the backup to the destination server
--------------------------------------------------------------------------------

Use :command:`rsync` or :command:`scp` to copy the data to the destination
server. If you're syncing the data directly to already running slave's data
directory it's advised to stop the |MySQL| server there.

.. code-block:: console

   TheMaster$ rsync -avprP -e ssh /path/to/backupdir/$TIMESTAMP NewSlave:/path/to/mysql/

After you copy data over, make sure |MySQL| has proper permissions to access them.

.. code-block:: console

   NewSlave$ chown mysql:mysql /path/to/mysql/datadir

STEP 4: Configure and start replication
--------------------------------------------------------------------------------

Following command will tell the new slave what was the last ``GTID`` executed on
the master when backup was taken.

.. code-block:: guess

   NewSlave > SET GLOBAL gtid_purged="c777888a-b6df-11e2-a604-080027635ef5:1-4";
   NewSlave > CHANGE MASTER TO 
                MASTER_HOST="$masterip", 
                MASTER_USER="repl",
                MASTER_PASSWORD="$slavepass",
                MASTER_AUTO_POSITION = 1;

STEP 5: Check the replication status
--------------------------------------------------------------------------------

Following command will show the slave status:

.. code-block:: guess

   NewSlave > show slave status\G
            [..]
            Slave_IO_Running: Yes
            Slave_SQL_Running: Yes
            [...]
            Retrieved_Gtid_Set: c777888a-b6df-11e2-a604-080027635ef5:5
            Executed_Gtid_Set: c777888a-b6df-11e2-a604-080027635ef5:1-5

We can see that the slave has retrieved a new transaction with number 5, so
transactions from 1 to 5 are already on this slave.

That's all, we have created a new slave in our ``GTID`` based replication environment.
