================================================================================
 Backing Up and Restoring Individual Partitions
================================================================================

|Percona XtraBackup| features :doc:`partial backups
<../innobackupex/partial_backups_innobackupex>`, which means that you may backup
individual partitions as well because from the storage engines perspective
partitions are regular tables with specially formatted names. The only
requirement for this feature is having the :term:`innodb_file_per_table` option
enabled in the server.

There is only one caveat about using this kind of backup: you can't copy back
the prepared backup. Restoring partial backups should be done by importing the
tables, and not by using the traditional :option:`innobackupex --copy-back`
option. Although there are some scenarios where restoring can be done by copying
back the files, this may be lead to database inconsistencies in many cases and
it is not the recommended way to do it.

Creating the backup
================================================================================

There are three ways of specifying which part of the whole data will be backed
up: regular expressions (:option:`innobackupex --include`), enumerating the
tables in a file (:option:`innobackupex --tables-file`) or providing a list of
databases (:option:`innobackupex --databases`). In this example
:option:`innobackupex --include` option will be used.

The regular expression provided to this option will be matched against the fully
qualified tablename, including the database name, in the form
``databasename.tablename``.

For example, this will back up the partition ``p4`` from the table ``name``
located in the database ``imdb``::

  $ innobackupex --include='^imdb[.]name#P#p4' /mnt/backup/

This will create a timestamped directory with the usual files that
|innobackupex| creates, but only the data files related to the tables matched.

Output of the |innobackupex| will list the skipped tables :: 
  
  ...
  [01] Skipping ./imdb/person_info.ibd
  [01] Skipping ./imdb/name#P#p5.ibd
  [01] Skipping ./imdb/name#P#p6.ibd
  ...
  imdb.person_info.frm is skipped because it does not match ^imdb[.]name#P#p4.
  imdb.title.frm is skipped because it does not match ^imdb[.]name#P#p4.
  imdb.company_type.frm is skipped because it does not match ^imdb[.]name#P#p4.
  ... 

Note that this option is passed to :option:`xtrabackup --tables` and is matched
against each table of each database, the directories of each database will be
created even if they are empty.

Preparing the backup
================================================================================

For preparing partial backups, the procedure is analogous to :doc:`restoring
individual tables <../innobackupex/restoring_individual_tables_ibk>` : apply the
logs and use :option:`innobackupex --export`:

.. code-block:: bash

   $ innobackupex --apply-log --export /mnt/backup/2012-08-28_10-29-09

You may see warnings in the output about tables that don't exists. This is
because |InnoDB|-based engines stores its data dictionary inside the tablespace
files besides the :term:`.frm` files. |innobackupex| will use |xtrabackup| to
remove the missing tables (those that haven't been selected in the partial
backup) from the data dictionary in order to avoid future warnings or errors::

  InnoDB: in InnoDB data dictionary has tablespace id 51,
  InnoDB: but tablespace with that id or name does not exist. It will be removed from data dictionary.
  120828 10:25:28  InnoDB: Waiting for the background threads to start
  120828 10:25:29 Percona XtraDB (http://www.percona.com) 1.1.8-20.1 started; log sequence number 10098323731
  xtrabackup: export option is specified.
  xtrabackup: export metadata of table 'imdb/name#P#p4' to file `./imdb/name#P#p4.exp` (1 indexes)
  xtrabackup:     name=PRIMARY, id.low=73, page=3

You should also see the notification of the creation of a file needed for
importing (:term:`.exp` file) for each table included in the partial backup::

  xtrabackup: export option is specified.
  xtrabackup: export metadata of table 'imdb/name#P#p4' to file `./imdb/name#P#p4.exp` (1 indexes)
  xtrabackup:     name=PRIMARY, id.low=73, page=3

Note that you can use :option:`innobackupex --export` with :option:`innobackupex --apply-log`
to an already-prepared backup in order to create the :term:`.exp`
files.

Finally, check the for the confirmation message in the output:: 

  120828 19:25:38  innobackupex: completed OK!

Restoring from the backups
================================================================================

Restoring should be done by :doc:`importing the tables
<../innobackupex/restoring_individual_tables_ibk>` in the partial backup to the
server.

.. note::

   Improved table/partition import is only available in |Percona Server| and
   |MySQL| 5.6, this means that partitions which were backed up from different
   server can be imported as well. For versions older than |MySQL| 5.6 only
   partitions from that server can be imported with some important
   limitations. There should be no DROP/CREATE/TRUNCATE/ALTER TABLE commands
   issued between taking the backup and importing the partition.

First step is to create new table in which data will be restored :: 

.. code-block:: guess

   mysql> CREATE TABLE `name_p4` (
   `id` int(11) NOT NULL AUTO_INCREMENT,
   `name` text NOT NULL,
   `imdb_index` varchar(12) DEFAULT NULL,
   `imdb_id` int(11) DEFAULT NULL,
   `name_pcode_cf` varchar(5) DEFAULT NULL,
   `name_pcode_nf` varchar(5) DEFAULT NULL,
   `surname_pcode` varchar(5) DEFAULT NULL,
   PRIMARY KEY (`id`)
   ) ENGINE=InnoDB AUTO_INCREMENT=2812744 DEFAULT CHARSET=utf8

To restore the partition from the backup tablespace needs to be discarded for
that table:

.. code-block:: guess

   mysql>  ALTER TABLE name_p4 DISCARD TABLESPACE;

The next step is to copy the :term:`.exp` and `ibd` files from the backup to |MySQL|
data directory:

.. code-block:: bash

   $ cp /mnt/backup/2012-08-28_10-29-09/imdb/name#P#p4.exp /var/lib/mysql/imdb/name_p4.exp
   $ cp /mnt/backup/2012-08-28_10-29-09/imdb/name#P#p4.ibd /var/lib/mysql/imdb/name_p4.ibd
 
.. note::

   Make sure that the copied files can be accessed by the user running the |MySQL|.

If you are running the |Percona Server| make sure that variable `innodb_import_table_from_xtrabackup` is enabled:

.. code-block:: guess

   mysql> SET GLOBAL innodb_import_table_from_xtrabackup=1;

The last step is to import the tablespace:

.. code-block:: guess

   mysql>  ALTER TABLE name_p4 IMPORT TABLESPACE;

Restoring from the backups in version 5.6
--------------------------------------------------------------------------------

The problem with server versions up to 5.5 is that there is no server support to
import either individual partitions or all partitions of a partitioned table, so
partitions could only be imported as independent tables. In |MySQL| and |Percona
Server| 5.6 it is possible to exchange individual partitions with independent
tables through ``ALTER TABLE`` ... ``EXCHANGE PARTITION`` command.

.. note:: 

  In |Percona Server| 5.6, the variable ``innodb_import_table_from_xtrabackup``
  was been removed in favor of |MySQL| `Transportable Tablespaces
  <http://dev.mysql.com/doc/refman/5.6/en/tablespace-copying.html>`_
  implementation.

When importing an entire partitioned table, first import all (sub)partitions as
independent tables:

.. code-block:: guess

   mysql> CREATE TABLE `name_p4` (
   `id` int(11) NOT NULL AUTO_INCREMENT,
   `name` text NOT NULL,
   `imdb_index` varchar(12) DEFAULT NULL,
   `imdb_id` int(11) DEFAULT NULL,
   `name_pcode_cf` varchar(5) DEFAULT NULL,
   `name_pcode_nf` varchar(5) DEFAULT NULL,
   `surname_pcode` varchar(5) DEFAULT NULL,
   PRIMARY KEY (`id`)
   ) ENGINE=InnoDB AUTO_INCREMENT=2812744 DEFAULT CHARSET=utf8

To restore the partition from the backup tablespace needs to be discarded for
that table:

.. code-block:: guess

   mysql>  ALTER TABLE name_p4 DISCARD TABLESPACE;

The next step is to copy the ``.cfg`` and ``.ibd`` files from the backup to |MySQL| data directory:

.. code-block:: bash

   $ cp /mnt/backup/2013-07-18_10-29-09/imdb/name#P#p4.cfg /var/lib/mysql/imdb/name_p4.cfg
   $ cp /mnt/backup/2013-07-18_10-29-09/imdb/name#P#p4.ibd /var/lib/mysql/imdb/name_p4.ibd

The last step is to import the tablespace:

.. code-block:: guess

   mysql>  ALTER TABLE name_p4 IMPORT TABLESPACE;

We can now create the empty partitioned table with exactly the same schema as
the table being imported:

.. code-block:: guess

   mysql> CREATE TABLE name2 LIKE name;

Then swap empty partitions from the newly created table with individual tables
corresponding to partitions that have been exported/imported on the previous
steps:

.. code-block:: guess

   mysql> ALTER TABLE name2 EXCHANGE PARTITION p4 WITH TABLE name_p4;

In order for this operation to be successful `following conditions
<http://dev.mysql.com/doc/refman/5.6/en/partitioning-management-exchange.html>`_
have to be met.
