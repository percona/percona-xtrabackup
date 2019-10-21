.. _replication_howto:

================================================================================
How to setup a slave for replication in 6 simple steps with Percona XtraBackup
================================================================================

Data is, by far, the most valuable part of a system. Having a backup done
systematically and available for a rapid recovery in case of failure is
admittedly essential to a system. However, it is not common practice because of
its costs, infrastructure needed or even the boredom associated to the
task. |Percona XtraBackup| is designed to solve this problem.

You can have almost real-time backups in 6 simple steps by setting up a
replication environment with |Percona XtraBackup|.

|Percona XtraBackup| is a tool for backing up your data extremely easy and
without interruption. It performs "hot backups" on unmodified versions of
|MySQL| servers (5.1, 5.5 and 5.6), as well as |MariaDB| and *Percona
Servers*. It is a totally free and open source software distributed only under
the *GPLv2* license.

All the things you will need
================================================================================

Setting up a slave for replication with |Percona XtraBackup| is really a very
straightforward procedure. In order to keep it simple, here is a list of the
things you need to follow the steps without hassles:

* ``TheMaster`` A system with a |MySQL|-based server installed, configured and
  running. This system will be called ``TheMaster``, as it is where your data is
  stored and the one to be replicated. We will assume the following about this
  system:

  * the |MySQL| server is able to communicate with others by the standard TCP/IP
    port;

  * the *SSH* server is installed and configured;

  * you have a user account in the system with the appropriate permissions;

  * you have a MySQL's user account with appropriate privileges.

  * server has binlogs enabled and server-id set up to 1.


* ``TheSlave`` Another system, with a |MySQL|-based server installed on it. We
  will refer to this machine as ``TheSlave`` and we will assume the same things
  we did about ``TheMaster``, except that the server-id on ``TheSlave`` is 2.

* ``Percona XtraBackup`` The backup tool we will use. It should be installed in
  both computers for convenience.

.. note::

   It is not recommended to mix MySQL variants (Percona Server, MySQL, MariaDB)
   in your replication setup.  This may produce incorrect
   :file:`xtrabackup_slave_info` file when adding a new slave.

STEP 1: Make a backup on ``TheMaster`` and prepare it
================================================================================

At ``TheMaster``, issue the following to a shell:

.. code-block:: console

   TheMaster$ xtrabackup --backup --user=yourDBuser --password=MaGiCdB1 --target-dir=/path/to/backupdir 

After this is finished you should get:

.. code-block:: console

   xtrabackup: completed OK! 

This will make a copy of your |MySQL| data dir to the :file:`/path/to/backupdir`
directory.  You have told |Percona XtraBackup| to connect to the database server
using your database user and password, and do a hot backup of all your data in
it (all |MyISAM|, |InnoDB| tables and indexes in them).

In order for snapshot to be consistent you need to prepare the data:

.. code-block:: console

   TheMaster$ xtrabackup --user=yourDBuser --password=MaGiCdB1 \
                --prepare --target-dir=/path/to/backupdir

You need to select path where your snapshot has been taken.  If everything is ok
you should get the same OK message.  Now the transaction logs are applied to the
data files, and new ones are created: your data files are ready to be used by
the MySQL server.

|Percona XtraBackup| knows where your data is by reading your :term:`my.cnf`.
If you have your configuration file in a non-standard place, you should use the
flag :option:`xtrabackup --defaults-file` ``=/location/of/my.cnf``.

If you want to skip writing the user name and password every time you want to
access |MySQL|, you can set it up in :file:`.mylogin.cnf` as follows::

 mysql_config_editor set --login-path=client --host=localhost --user=root --password

This will give you the root access to MySQL. 

.. seealso::

   |MySQL| Documentaiton: MySQL Configuration Utility
      https://dev.mysql.com/doc/refman/5.7/en/mysql-config-editor.html

STEP 2:  Copy backed up data to TheSlave
================================================================================

Use ``rsync`` or ``scp`` to copy the data from Master to Slave. If you're
syncing the data directly to slave's data directory it's advised to stop the
mysqld there.

.. code-block:: console

   TheMaster$ rsync -avpP -e ssh /path/to/backupdir TheSlave:/path/to/mysql/

After data has been copied you can back up the original or previously installed
|MySQL| :term:`datadir` (**NOTE**: Make sure mysqld is shut down before you move
the contents of its datadir, or move the snapshot into its datadir.):

.. code-block:: console

   TheSlave$ mv /path/to/mysql/datadir /path/to/mysql/datadir_bak

and move the snapshot from ``TheMaster`` in its place:

.. code-block:: console

   TheSlave$ xtrabackup --move-back --target-dir=/path/to/mysql/backupdir

After you copy data over, make sure |MySQL| has proper permissions to access them.

.. code-block:: console

   TheSlave$ chown mysql:mysql /path/to/mysql/datadir

In case the ibdata and iblog files are located in different directories outside
of the datadir, you will have to put them in their proper place after the logs
have been applied.

STEP 3: Configure The Master's MySQL server
================================================================================

Add the appropriate grant in order for slave to be able to connect to master: 

.. code-block:: guess

   TheMaster|mysql> GRANT REPLICATION SLAVE ON *.*  TO 'repl'@'$slaveip'
   IDENTIFIED BY '$slavepass';

Also make sure that firewall rules are correct and that ``TheSlave`` can connect
to ``TheMaster``. Test that you can run the mysql client on ``TheSlave``,
connect to ``TheMaster``, and authenticate. ::

  TheSlave$ mysql --host=TheMaster --user=repl --password=$slavepass

Verify the privileges. ::  

  mysql> SHOW GRANTS;

STEP 4: Configure The Slave's MySQL server
================================================================================

First copy the :term:`my.cnf` file from ``TheMaster`` to ``TheSlave``:

.. code-block:: console

   TheSlave$ scp user@TheMaster:/etc/mysql/my.cnf /etc/mysql/my.cnf

then change the following options in /etc/mysql/my.cnf:

.. code-block:: console

   server-id=2

and start/restart :command:`mysqld` on ``TheSlave``.

In case you're using init script on Debian based system to start mysqld, be sure
that the password for ``debian-sys-maint`` user has been updated and it's the
same as that user's password on the ``TheMaster``. Password can be seen and
updated in :file:`/etc/mysql/debian.cnf`.

STEP 5: Start the replication
================================================================================

Look at the content of the file :file:`xtrabackup_binlog_info`, it will be something like:

.. code-block:: console

   TheSlave$ cat /var/lib/mysql/xtrabackup_binlog_info
   TheMaster-bin.000001     481

Execute the ``CHANGE MASTER`` statement on a MySQL console and use the username and password you've set up in STEP 3: 

.. code-block:: guess

   TheSlave|mysql> CHANGE MASTER TO 
                   MASTER_HOST='$masterip',	
                   MASTER_USER='repl',
                   MASTER_PASSWORD='$slavepass',
                   MASTER_LOG_FILE='TheMaster-bin.000001', 
                   MASTER_LOG_POS=481;

and start the slave:

.. code-block:: guess

   TheSlave|mysql> START SLAVE;

STEP 6: Check
================================================================================

You should check that everything went OK with:

.. code-block:: guess

   TheSlave|mysql> SHOW SLAVE STATUS \G
            ...
            Slave_IO_Running: Yes
            Slave_SQL_Running: Yes
            ...
            Seconds_Behind_Master: 13
            ...

Both ``IO`` and ``SQL`` threads need to be running. The
``Seconds_Behind_Master`` means the ``SQL`` currently being executed has a
``current_timestamp`` of 13 seconds ago. It is an estimation of the lag between
``TheMaster`` and ``TheSlave``. Note that at the beginning, a high value could
be shown because ``TheSlave`` has to "catch up" with ``TheMaster``.

Adding more slaves to The Master
================================================================================

You can use this procedure with slight variation to add new slaves to a
master. We will use |Percona XtraBackup| to clone an already configured
slave. We will continue using the previous scenario for convenience but we will
add ``TheNewSlave`` to the plot.

At ``TheSlave``, do a full backup:

.. code-block:: console

   TheSlave$ xtrabackup --user=yourDBuser --password=MaGiCiGaM \
   --backup --slave-info --target-dir=/path/to/backupdir

By using the :option:`xtrabackup --slave-info` |Percona XtraBackup| creates
additional file called :file:`xtrabackup_slave_info`.

Apply the logs:

.. code-block:: console

   TheSlave$ xtrabackup --prepare --use-memory=2G --target-dir=/path/to/backupdir/

Copy the directory from the ``TheSlave`` to ``TheNewSlave`` (**NOTE**: Make sure
mysqld is shut down on ``TheNewSlave`` before you copy the contents the snapshot
into its :term:`datadir`.):

.. code-block:: console

   rsync -avprP -e ssh /path/to/backupdir TheNewSlave:/path/to/mysql/datadir

Add additional grant on the master:

.. code-block:: guess

	TheMaster|mysql> GRANT REPLICATION SLAVE ON *.*  TO 'repl'@'$newslaveip'
                     IDENTIFIED BY '$slavepass';

Copy the configuration file from ``TheSlave``:

.. code-block:: console

   TheNEWSlave$ scp user@TheSlave:/etc/mysql/my.cnf /etc/mysql/my.cnf

Make sure you change the server-id variable in :file:`/etc/mysql/my.cnf` to 3
and disable the replication on start:

.. code-block:: console

   skip-slave-start
   server-id=3

After setting ``server_id``, start :command:`mysqld`.

Fetch the master_log_file and master_log_pos from the file
:file:`xtrabackup_slave_info`, execute the statement for setting up the master
and the log file for ``The NEW Slave``:

.. code-block:: guess

   TheNEWSlave|mysql> CHANGE MASTER TO 
                      MASTER_HOST='$masterip',
                      MASTER_USER='repl',
                      MASTER_PASSWORD='$slavepass',
                      MASTER_LOG_FILE='TheMaster-bin.000001', 
                      MASTER_LOG_POS=481;

and start the slave:

.. code-block:: guess

   TheNEWSlave|mysql> START SLAVE;

If both IO and SQL threads are running when you check the ``TheNewSlave``,
server is replicating ``TheMaster``.
