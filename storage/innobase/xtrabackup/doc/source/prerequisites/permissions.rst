
.. _pxb.privilege:


Permissions and Privileges Needed
================================================================================

For the purposes of this manual, *permissions* refer to the ability of a user to access and perform changes on the relevant parts of the host's filesystem, such as starting or stopping services, or installing software.

*Privileges* refer to the abilities of a database user to perform different kinds of actions on the database server.

Permissions at a system level
---------------------------------------------

Once connected to the server, in order to perform a backup you must have the
``READ`` and ``EXECUTE`` permissions at a filesystem level in the
server's `datadir`.

Checking for Permissions
+++++++++++++++++++++++++++++++

There are many ways for checking the permission on a file or directory. For example, ``ls -ls /path/to/file`` or ``stat /path/to/file | grep Access`` will do the job: ::

  $ stat /etc/mysql | grep Access
  Access: (0755/drwxr-xr-x)  Uid: (    0/    root)   Gid: (    0/    root)
  Access: 2011-05-12 21:19:07.129850437 -0300
  $ ls -ld /etc/mysql/my.cnf 
  -rw-r--r-- 1 root root 4703 Apr  5 06:26 /etc/mysql/my.cnf

As in this example, ``my.cnf`` is owned by ``root`` and not writable for anyone else. Assuming that you do not have the password for ``root``, you can check what permissions you have on this file type with ``sudo -l``: ::

  $ sudo -l
  Password:
  You may run the following commands on this host:
  (root) /usr/bin/
  (root) NOPASSWD: /etc/init.d/mysqld
  (root) NOPASSWD: /bin/vi /etc/mysql/my.cnf
  (root) NOPASSWD: /usr/local/bin/top
  (root) NOPASSWD: /usr/bin/ls
  (root) /bin/tail

Using ``sudo`` in ``/etc/init.d/``, ``/etc/rc.d/`` or ``/sbin/service`` script provides the ability to start and stop services.

Also, if you can execute the package manager of your distribution, you can install or remove software with it. If not, having ``rwx`` permission over a directory will let you do a local installation of software by compiling it there. This is a typical situation in many companies.

There are other ways for managing permissions, such as using *PolicyKit*, *Extended ACLs* or *SELinux*, which may be preventing or allowing your access. You should check them in that case.

Privileges Needed
------------------------------

The database user needs the following privileges on the tables or databases:

* ``RELOAD`` and ``LOCK TABLES`` (unless the `--no-lock <--no-lock>`
  option is specified) to run :mysql:`FLUSH TABLES WITH READ LOCK` and
  :mysql:`FLUSH ENGINE LOGS` before you copy the files. When `Backup Locks
  <http://www.percona.com/doc/percona-server/8.0/management/backup_locks.html>`_
  are used these privileges are required. 

* ``BACKUP_ADMIN`` to query the
  performance_schema.log_status table, and run :mysql:`LOCK INSTANCE FOR BACKUP`,
  :mysql:`LOCK BINLOG FOR BACKUP`, or :mysql:`LOCK TABLES FOR BACKUP`.

* ``REPLICATION CLIENT`` to obtain the binary log position,

* ``CREATE TABLESPACE`` to import tables (see :ref:`pxb.xtrabackup.table.restoring`),

* ``PROCESS`` to run ``SHOW ENGINE INNODB STATUS`` which is
  mandatory and, optionally, to see the threads running on the
  server (see :ref:`pxb.xtrabackup.flush-tables-with-read-lock`),

* ``SUPER`` to start or to stop the replication threads in a replication
  environment when :ref:`handling FLUSH TABLES WITH READ LOCK <pxb.xtrabackup.flush-tables-with-read-lock>`,

* ``CREATE`` to create the
  :ref:`PERCONA_SCHEMA.xtrabackup_history <xtrabackup_history>` database and
  table,

* ``ALTER`` to upgrade the
  :ref:`PERCONA_SCHEMA.xtrabackup_history <xtrabackup_history>` database and
  table,

* ``INSERT`` to add history records to the
  :ref:`PERCONA_SCHEMA.xtrabackup_history <xtrabackup_history>` table,

* ``SELECT`` to use :option:`--incremental-history-name` or
  :option:`--incremental-history-uuid` 
  to look up the ``innodb_to_lsn`` values in the
  :ref:`PERCONA_SCHEMA.xtrabackup_history <xtrabackup_history>` table.

* ``SELECT`` on the `keyring_component_status table <https://dev.mysql.com/doc/refman/8.0/en/performance-schema-keyring-component-status-table.html>`__  to view the attributes and status of the installed keyring component when in use.

The explanation of when to use these privileges is found in
:ref:`how_xtrabackup_works`.

Creating a User with minimum privileges
+++++++++++++++++++++++++++++++++++++++++++++

The following is an example of a SQL script which creates a database user with the minimum set of privileges needed to make full backup:

.. code-block:: mysql

   mysql> CREATE USER 'bkpuser'@'localhost' IDENTIFIED BY 's3cr%T'; 
   mysql> GRANT BACKUP_ADMIN, PROCESS, RELOAD, LOCK TABLES, REPLICATION CLIENT ON *.* TO 'bkpuser'@'localhost';
   mysql> GRANT SELECT ON performance_schema.log_status TO 'bkpuser'@'localhost';
   mysql> GRANT SELECT ON performance_schema.keyring_component_status TO bkpuser@'localhost' 
   mysql> FLUSH PRIVILEGES;

Checking the Privileges
++++++++++++++++++++++++++++++++++++++

To query the privileges that your database user has been granted, at a console of the server execute: ::

  mysql> SHOW GRANTS;

or for a particular user with: ::

  mysql> SHOW GRANTS FOR 'db-user'@'host';

It will display the privileges using the same format as for the `GRANT statement <http://dev.mysql.com/doc/refman/8.0/en/show-grants.html>`_.

Note that privileges may vary across versions of the server. To list the exact list of privileges that your server supports (and a brief description of them) execute: ::

  mysql> SHOW PRIVILEGES;






