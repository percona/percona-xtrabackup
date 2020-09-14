.. _pxb.replication.setting-up:

================================================================================
Steps for setting up replication
================================================================================

:Overview: :ref:`pxb.replication`

.. contents::
   :local:


.. _pxb.replication.setting-up/replication-source.backing-up:

Make a backup on the replication source
================================================================================

Set up your replication source as described in |mysql| documentation:

- `Set a unique server_id
  <https://dev.mysql.com/doc/refman/8.0/en/replication-howto-masterbaseconfig.html>`_
- `Set up a replication user
  <https://dev.mysql.com/doc/refman/8.0/en/replication-howto-repuser.html>`_ 

Now, you are ready to make a backup of the data stored on the replication
source:

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/data/backups/

The destination directory |file.data.backups| contains the file
:file:`xtrabackup_binlog_info`. This file stores both binary log
coordinates and the |abbr.gtid| (provided the `GTID mode`_ is enabled).

.. code-block:: bash

   $ cat xtrabackup_binlog_info
   mysql-bin.000002    1232        c777888a-b6df-11e2-a604-080027635ef5:1-4

This information is also printed to standard output after the backup is made.


.. _pxb.replication.setting-up/backup.preparing:

Prepare the backup for restoring on a replica
================================================================================

The backup will be prepared with the following command on the replication source: 

.. code-block:: console

   $ xtrabackup --prepare --target-dir=/data/backup

You need to specify the path where your snapshot has been taken
(|file.data.backups|) as the value of the |param.target-dir| parameter. If
everything is OK, you should get the same OK message. Now, the transaction logs
are applied to the data files, and new ones are created: your data files are
ready to be used by the MySQL server.

.. admonition:: More information

   :ref:`preparing_a_backup`

.. _pxb.replication.setting-up/backup.replica.moving:

Move the backup to a replica
================================================================================

Set up your replica as described in |mysql| documentation:

- `Set a server_id unique accross the whole replication configuration
  <https://dev.mysql.com/doc/refman/8.0/en/replication-howto-slavebaseconfig.html>`_
- `Set up a replication user
  <https://dev.mysql.com/doc/refman/8.0/en/replication-howto-repuser.html>`_

Use |cmd.rsync| or |cmd.scp| to copy the data to the destination
server. If you are synchronizing the data directly to the data directory of an
already running replica, you should first stop the |MySQL| server on this
replica.

.. code-block:: bash

   $ rsync -avprP -e ssh /path/to/backupdir \
   TargetReplicaIPAddress:/path/to/mysql/datadir

On the replica, set proper permissions on the directory that contains
the copied files so that the |MySQL| server is able to access them:

.. code-block:: bash

   $ chown mysql:mysql /path/to/mysql/datadir

If your replication configuration is based on binary logging you can start the
replica with the |sql.start-slave| command:

.. code-block:: guess

   mysql> CHANGE MASTER TO 
   MASTER_HOST='{REPLICATION_SOURCE_IP_ADDRESS}',	
   MASTER_USER='{REPLICATION_USER}',
   MASTER_PASSWORD='{REPLICATION_USER_PASSWORD}',
   MASTER_LOG_FILE='{BINLOG_FILE_FROM_XTRABACKUP_BINLOG_INFO}', 
   MASTER_LOG_POS={POSITION_FROMXTRABACKUP_BINLOG_INFO};

   mysql > START SLAVE;
   mysql > SHOW SLAVE STATUS;
   ...
   Slave_IO_Running: Yes
   Slave_SQL_Running: Yes
   ...

The |sql.show-slave-status| command informs that your replica has started
successfully.


.. _pxb.replication.setting-up/replication.starting:

Start the |abbr.gtid| based replication on the replica
================================================================================

In case, your replication configuration uses |abbr.gtid|, you need to set the
`gtid_purged` variable to the value of ``GTID`` from
|file.xtrabackup-binlog-info| in the copied backup. Then, update the information
about the replication source and start your replica:

.. code-block:: guess

   # Using the mysql shell
   mysql > SET GLOBAL gtid_purged='{GTID_STRING_FOUND_IN_XTRABACKUP_BINLOG_INFO}';
   
   mysql > CHANGE MASTER TO 
   MASTER_HOST="{REPLICATION_SOURCE_IP_ADDRESS}", 
   MASTER_USER="{REPLICATION_USER}",
   MASTER_PASSWORD="{REPLICATION_USER_PASSWORD}",
   MASTER_AUTO_POSITION = 1;

   mysql > START SLAVE;

.. seealso:: 

   |mysql| documentation:
      - `gtid_purged variable
	<https://dev.mysql.com/doc/refman/8.0/en/replication-gtids-lifecycle.html#replication-gtids-gtid-purged>`_
   |percona-blog|:
      - `How to create/restore a slave using GTID replication in MySQL 5.6
	<https://www.percona.com/blog/2013/02/08/how-to-createrestore-a-slave-using-gtid-replication-in-mysql-5-6/>`_

If you use |pxc| on your nodes, set ``wsrep_on`` variable to `0` on the replica
before running the |sql.reset-master| command. The reason is that |pxc| does not
allow using |sql.reset-master| if ``wsrep_on`` is set to `1`.

.. code-block:: guess

   mysql > SET SESSION wsrep_on = 0;
   mysql > RESET MASTER;
   mysql > SET SESSION wsrep_on = 1;
   mysql > SET GLOBAL gtid_purged='{GTID_STRING_FOUND_IN_XTRABACKUP_BINLOG_INFO}';
   mysql > CHANGE MASTER TO 
                MASTER_HOST="{REPLICATION_SOURCE_IP_ADDRESS}", 
                MASTER_USER="{REPLICATION_USER}",
                MASTER_PASSWORD="{REPLICATION_USER_PASSWORD}",
                MASTER_AUTO_POSITION = 1;
   mysql > START SLAVE;

.. seealso:: 

   |pxc| documentation:
      - `wsrep_on variable
	<https://www.percona.com/doc/percona-xtradb-cluster/LATEST/wsrep-system-index.html#wsrep_on>`_

The following command shows the status of the replica:

.. code-block:: guess

   mysql > SHOW SLAVE STATUS\G
            [..]
            Slave_IO_Running: Yes
            Slave_SQL_Running: Yes
            [...]
            Retrieved_Gtid_Set: c777888a-b6df-11e2-a604-080027635ef5:5
            Executed_Gtid_Set: c777888a-b6df-11e2-a604-080027635ef5:1-5

The replica has retrieved a new transaction with number 5, so
transactions from 1 to 5 are already on this replica. We have successfully
created a new replica in our |abbr.gtid| based replication environment.



-----

.. admonition:: Related information

   - :ref:`pxb.replication`

.. Replacements ================================================================

.. include:: ../_res/links.txt
.. include:: ../_res/replace/abbr.txt
.. include:: ../_res/replace/file.txt
.. include:: ../_res/replace/parameter.txt
.. include:: ../_res/replace/proper.txt
.. include:: ../_res/replace/sql.txt
.. include:: ../_res/replace/text.txt
