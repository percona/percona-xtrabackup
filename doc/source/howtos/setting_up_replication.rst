.. _replication_howto:

========================================================================
 How to setup a slave for replication in 5 simple steps with Xtrabackup
========================================================================

  Data is, by far, the most valuable part of a system. Having a backup done systematically and available for a rapid recovery in case of failure is admittedly essential to a system. However, it is not common practice because of its costs, infrastructure needed or even the boredom associated to the task. Xtrabackup is designed to solve this problem.

  You can have almost real-time backups in 5 simple steps by setting up a replication environment with |XtraBackup|. 

 *Percona* |XtraBackup| is a tool for backing up your data extremely easy and without interruption. It performs "hot backups" on unmodified versions of |MySQL| servers (5.0 and 5.1), as well as |MariaDB| and *Percona Servers*. It is a totally free and open source software distributed only under the *GPLv2* license.

All the things you will need
============================

Setting up a slave for replication with |XtraBackup| is really a very straightforward procedure. In order to keep it simple, here is a list of the things you need to follow the steps without hassles:

* ``The Master`` 
  A system with a |MySQL|-based server installed, configured and running. This system will be called ``The Master``, as it is where your data is stored and the one to be replicated. We will assume the following about this system:

  * the |MySQL| server is able to communicate with others by the standard TCP/IP port;

  * the *SSH* server is installed and configured;

  * you have an user account in the system with the appropiate permissions;

  * you have a MySQL's user account with appropiate privileges.


* ``The Slave`` 
  Another system, with a |MySQL|-based server installed on it. We will reffer to this machine as ``The Slave`` and we will assume the same things we did about ``The Master``.

* ``Xtrabackup``
  The backup tool we will use. It should be installed in both computers for convenience.

STEP 1: Make a backup on ``The Master`` and stream it to ``The Slave``
======================================================================

At ``The Master``, issue the following to a shell:

.. code-block:: console

   TheMaster$ innobackupex --user=yourDBuser --password=MaGiCdB1 \ 
               --stream=tar /tmp/ | \
               ssh user@THESLAVE "tar xfi - -C /var/lib/mysql"

You have told |XtraBackup| (through the |innobackupex| script) to connect to the database server using your database user and password, and do a hot backup of all your data in it (all |MyISAM|, |InnoDB| tables and indexes in them).

Instead of writing it to a file, we printed it to ``STDOUT``, so it can be piped - via :command:`ssh` - to :command:`tar` in ``The Slave``, which will extract the files directly to ``The Slave`` 's data directory.

|XtraBackup| knows where your data is by reading your :term:`my.cnf`. If you have your configuration file in a non-standard place, you should use the flag :option:`--defaults-file` ``=/location/of/my.cnf``.

If, instead of ``--stream=tar /tmp/``, you give a directory to |innobackupex|, then the backup will be kept in ``The Master`` 's filesystem. A subdirectory will be created on the path you gave it and its name will be the current timestamp. You should copy that entire subdirectory to ``The Slave``, for example, with ``scp -r 2011-12-31_23-59-59``.

STEP 2: Apply the logs to the data
==================================

At ``The Slave``, tell |XtraBackup| to apply the binary logs to the data with:

.. code-block:: console

   TheSlave$ innobackupex --apply-log --use-memory=2G  /var/lib/mysql

Now the transaction logs are applied to the data files, and new ones are created: your data files are ready to be used by the MySQL server. 

The :option:`--use-memory` ``=2G`` option indicates Xtrabackup to use 2 Gigabytes of RAM for the process. Adjust this amount to your situation in order to speed up the task. 

STEP 3: Configure The Slave's MySQL server
==========================================

First copy the :term:`my.cnf` file from ``The Master`` to ``The Slave``:

.. code-block:: console

   TheSlave$ scp user@TheMaster:/etc/mysql/my.cnf /etc/mysql/my.cnf

and start/restart :command:`mysqld` on ``The Slave``.

You can do this without problems because you are replicating the "whole server". As it is the first slave that ``The Master`` has, |MySQL| will do the rest automatically.

STEP 4: Start the replication
=============================

Look at the content of the file :file:`xtrabackup_binlog_info`, it will be something like:

.. code-block:: console

   TheSlave$ cat /var/lib/mysql/xtrabackup_binlog_info
    TheMaster-bin.000001     481

Execute the ``CHANGE MASTER`` statement on a mysql console but remember to add to it your database user and password in ``The Master``:

.. code-block:: mysql

   TheSlave|mysql> CHANGE MASTER TO 
                   MASTER_USER='DBuserInTheMaster',
                   MASTER_PASSWORD='m4g1cM4st3r',
                   MASTER_LOG_FILE='TheMaster-bin.000001', 
                   MASTER_LOG_POS=481;

and start the slave:

.. code-block:: mysql

   TheSlave|mysql> START SLAVE;

STEP 5: Check
=============

You should check that everything went OK with:

.. code-block:: mysql

   TheSlave|mysql> SHOW SLAVE STATUS \G
            ...
            Slave_IO_Running: Yes
            Slave_SQL_Running: Yes
            ...
            Seconds_Behind_Master: 13
            ...

The ``Seconds_Behind_Master`` means the ``SQL`` currently being executed has a ``current_timestamp`` of 13 seconds ago. It is an estimation of the lag between ``The Master`` and ``The Slave``. Note that at the begining, a high value should be shown because ``The Slave`` has to "catch up" with ``The Master``.

Adding more slaves to The Master
================================

You can use this procedure with slights variation to add new slaves to a master. We will use |Xtrabackup| to clone an already configured slave. We will continue using the previuos scenario for convenience but we will add ``The NEW Slave`` to the plot.

At ``The Slave``, do a full backup and stream it to ``The New Slave``:

.. code-block:: console

   TheSlave$ innobackupex --user=yourDBuser --password=MaGiCiGaM \ 
               --stream=tar /tmp/ --slave-info | \
               ssh user@TheNEWSlave "tar xfi - -C /var/lib/mysql"

and apply the logs on ``The NEW Slave``:

.. code-block:: console

   TheNEWSlave$ innobackupex --apply-log --use-memory=2G /var/lib/mysql

and copy the configuration file from ``The Slave``:

.. code-block:: console

   TheNEWSlave$ scp user@TheSlave:/etc/mysql/my.cnf /etc/mysql/my.cnf

Now comes a slight variation on the procedure: before starting the |MySQL| server, set the variable ``server_id`` in :term:`my.cnf` to a value greater than 2, for example, with

.. code-block:: console

   TheNEWSlave$ echo '\nSERVER_ID=3' >> /etc/mysql/my.cnf

This is because on a replication environment with one master and one slave, if the variable is not setted (as default) |MySQL| assumes 1 for ``server_id`` in the master and 2 in the slave. If you don't change it, |MySQL| will assume a value of 2 in ``The New Slave`` again: the replication will fail because all servers involved must have a different ``server_id``.

After setting ``server_id``, start :command:`mysqld`.

As we backed up a slave that was doing a replication, in a :command:`mysql` console tell the server to stop doing it:

.. code-block:: mysql

   TheNEWSlave|mysql> STOP SLAVE;

With the help of the file :file:`xtrabackup_slave_info`, execute the statement for setting up the master and the log file for ``The NEW Slave``:

.. code-block:: mysql

   TheNEWSlave|mysql> CHANGE MASTER TO 
                      MASTER_USER='DBuserInTheMaster',
                      MASTER_PASSWORD='m4g1cM4st3r',
                      MASTER_LOG_FILE='TheMaster-bin.000001', 
                      MASTER_LOG_POS=481;

and start the slave:

.. code-block:: mysql

   TheSlave|mysql> START SLAVE;

and we have a The NEW Slave replicating The Master.
