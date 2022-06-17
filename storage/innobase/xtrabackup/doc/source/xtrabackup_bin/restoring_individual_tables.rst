.. _export_import_tables:

=============================
 Restoring Individual Tables
=============================

With *Percona XtraBackup*, you can export individual tables from any *InnoDB* database, and 
import them into *Percona Server for MySQL* with *XtraDB* or *MySQL* 5.7. The source does not 
need to be *XtraDB* or *MySQL* 5.7 but the destination must be. This operation only works on 
individual `.ibd` files. A table that is not contained in its own `.ibd` file cannot be exported.


Let's see how to export and import the following table:

.. code-block:: mysql

  CREATE TABLE export_test (
    a int(11) DEFAULT NULL
  ) ENGINE=InnoDB DEFAULT CHARSET=latin1;

Exporting the Table
===================

This table should have been created in `innodb_file_per_table` mode, so
after taking a backup as usual with `xtrabackup --backup`, the
`.ibd` file should exist in the target directory:

.. code-block:: bash

  $ find /data/backups/mysql/ -name export_test.*
  /data/backups/mysql/test/export_test.ibd

when you prepare the backup, add the extra parameter
`xtrabackup --export` to the command. Here is an example:

.. code-block:: bash

  $ xtrabackup --prepare --export --target-dir=/data/backups/mysql/

.. note::

  If you're trying to restore :ref:`encrypted InnoDB tablespace
  <encrypted_innodb_tablespace_backups>` table you must specify the
  keyring file as well:

  .. code-block:: bash

    xtrabackup --prepare --export --target-dir=/tmp/table \
    --keyring-file-data=/var/lib/mysql-keyring/keyring

Now you should see a `.exp` file in the target directory:

.. code-block:: bash

  $ find /data/backups/mysql/ -name export_test.*
  /data/backups/mysql/test/export_test.exp
  /data/backups/mysql/test/export_test.ibd
  /data/backups/mysql/test/export_test.cfg


These three files are all you need to import the table into a server running
*Percona Server for MySQL* with XtraDB or *MySQL* 5.7. In case server is using `InnoDB
Tablespace Encryption
<http://dev.mysql.com/doc/refman/5.7/en/innodb-tablespace-encryption.html>`_
additional `.cfp` file be listed for encrypted tables.

.. note::

  *MySQL* uses `.cfg` file which contains *InnoDB* dictionary dump in
  special format. This format is different from the `.exp`` one which is
  used in XtraDB for the same purpose. Strictly speaking, a `.cfg``
  file is not required to import a tablespace to *MySQL* 5.7 or *Percona Server for MySQL* 5.7. A tablespace will be imported successfully even if it is from
  another server, but *InnoDB* will do schema validation if the corresponding
  `.cfg` file is present in the same directory.

Importing the Table
===================


On the destination server, create a table with the same structure, and then perform the following steps:

* Execute ``ALTER TABLE test.export_test DISCARD TABLESPACE;``

  * If you see the ``ERROR 1030
    (HY000): Got error -1 from storage engine`` message, then enable
    :term:`innodb_file_per_table` and create the table again: 

* Copy the exported files to the ``test/`` subdirectory of the destination
  server's data directory

* Execute ``ALTER TABLE test.export_test IMPORT TABLESPACE;``

The table should now be imported, and you should be able to ``SELECT`` from it
and see the imported data.

.. note::

  Persistent statistics for imported tablespace will be empty until you run the
  ``ANALYZE TABLE`` on the imported table. They are empty because they are
  stored in the system tables ``mysql.innodb_table_stats`` and
  ``mysql.innodb_index_stats`` and they are not updated by server during the
  import. This is due to upstream bug :mysqlbug:`72368`.
