.. _pxb.partial-backup:

================================================================================
Partial Backups
================================================================================

|xtrabackup| supports taking partial backups when the
:term:`innodb_file_per_table` option is enabled. There are three ways to create
partial backups:

1. matching the tables names with a regular expression
2. providing a list of table names in a file
3. providing a list of databases

.. warning::

   Do not copy back the prepared backup.

   Restoring partial backups should be done by importing the tables,
   not by using the :option:`--copy-back` option. It is not
   recommended to run incremental backups after running a partial
   backup.

   Although there are some scenarios where restoring can be done by
   copying back the files, this may lead to database
   inconsistencies in many cases and it is not a recommended way to
   do it.

For the purposes of this manual page, we will assume that there is a database
named ``test`` which contains tables named ``t1`` and ``t2``.

.. warning::

   If any of the matched or listed tables is deleted during the backup,
   |xtrabackup| will fail.

Creating Partial Backups
================================================================================

There are multiple ways of specifying which part of the whole data is backed up:

* Use the :option:`--tables` option to list the table names

* Use the :option:`--tables-file` option to list the tables in a file

* Use the :option:`--databases` option to list the databases

* Use the :option:`--databases-file` option to list the databases

The :option:`--tables` Option
================================================================================
This method involves the :option:`--tables` option. The value is a regular expression that is matched against a fully-qualified
tablename, including the database name, in the form ``databasename.tablename``.

To back up only tables in the ``test`` database, use the following
command:

.. code-block:: bash

  $ xtrabackup --backup --datadir=/var/lib/mysql --target-dir=/data/backups/ \
  --tables="^test[.].*"
  
To back up only the ``test.t1`` table, use the following command:

.. code-block:: bash

  $ xtrabackup --backup --datadir=/var/lib/mysql --target-dir=/data/backups/ \
  --tables="^test[.]t1"

The :option:`--tables-file` Option
================================================================================

The ``--tables-file`` option specifies a file that can contain multiple table
names, one table name per line in the file. Only the tables named in the file
will be backed up. Names are matched exactly, case-sensitive, with no pattern or
regular expression matching. The table names must be fully-qualified in
``databasename.tablename`` format.

.. code-block:: bash

  $ echo "mydatabase.mytable" > /tmp/tables.txt
  $ xtrabackup --backup --tables-file=/tmp/tables.txt 

The :option:`--databases` Option
================================================================================

The :option:`--databases` option accepts a space-separated list of the databases
and tables to backup in the format ``databasename[.tablename]``. In addition to
the listed databases, add the ``mysql``, ``sys``, and
``performance_schema`` databases. These databases are required when restoring
the databases using the :option:`--copy-back` option.

.. code-block:: bash

   $ xtrabackup --databases='mysql sys performance_schema test ...'
   
The :option:`--databases-file` Option
======================================================

The :option:`--databases-file` option specifies a file that can contain multiple
databases and tables in the ``databasename[.tablename]`` form, one element name
for each line in the file. Only the named databases and tables are backed up. These names are matched exactly and must be case-sensitive. This list does not match by  pattern or regular expression.

Preparing Partial Backups
================================================================================

The procedure is analogous to :ref:`restoring individual tables
<restoring_individual_tables>` : apply the logs and use the
:option:`--export` option:

.. code-block:: bash

   $ xtrabackup --prepare --export --target-dir=/path/to/partial/backup


If you use the :option:`--prepare` option on a partial backup, and you see warnings for tables that do not exist. These tables exist in the data dictionary inside InnoDB, but their corresponding :term:`.ibd` files do not exist. hese files were not copied into the backup directory but the tables exist in the data dictionary. The tables are removed from the data dictionary during the process and do not cause errors or warning in subsequent backups and restores.

An example of an error message during the prepare phase follows. ::

  Could not find any file associated with the tablespace ID: 4

  Could not find any file associated with the tablespace ID: 5

  Use --innodb-directories to find the tablespace files. If that fails then use --innodb-force-recovery=1 to ignore this and to permanently lose all changes to the missing tablespace(s).


Restoring Partial Backups
================================================================================

Restoring should be done by :ref:`restoring individual tables
<restoring_individual_tables>` in the partial backup to the server.

It can also be done by copying back the prepared backup to a "clean"
:term:`datadir` (in that case, make sure to include the ``mysql``
database) to the datadir you are moving the backup to. A system database can be created with the following:

.. code-block:: bash

   $ sudo mysql --initialize --user=mysql

Once you start the server, you may see mysql complaining about missing tablespaces:

.. sourcecode:: mysql

      2021-07-19T12:42:11.077200Z 1 [Warning] [MY-012351] [InnoDB] Tablespace 4, name 'test1/t1', file './d2/test1.ibd' is missing!
      2021-07-19T12:42:11.077300Z 1 [Warning] [MY-012351] [InnoDB] Tablespace 4, name 'test1/t1', file './d2/test1.ibd' is missing!

In order to clean the orphan database from the data dictionary, you must manually create the missing database directory and then ``DROP`` this database from the server. 

Example of creating the missing database:

.. sourcecode:: bash

      $ mkdir /var/lib/mysql/test1/d2

Example of dropping the database from the server:

.. sourcecode:: mysql

      mysql> DROP DATABASE d2;
      Query OK, 2 rows affected (0.5 sec)


