.. _export_import_tables:

=============================
 Restoring Individual Tables
=============================

In server versions prior to 5.6, it is not possible to copy tables between
servers by copying the files, even with :term:`innodb_file_per_table`. However,
with |Percona XtraBackup|, you can export individual tables from any |InnoDB|
database, and import them into |Percona Server| with |XtraDB| or |MySQL| 5.6.
(The source doesn't have to be |XtraDB| or or |MySQL| 5.6, but the destination
does.) This only works on individual :term:`.ibd` files, and cannot export a
table that is not contained in its own :term:`.ibd` file.

Let's see how to export and import the following table:

.. code-block:: mysql

  CREATE TABLE export_test (
    a int(11) DEFAULT NULL
  ) ENGINE=InnoDB DEFAULT CHARSET=latin1;

.. note::

   If you're running |Percona Server| version older than 5.5.10-20.1, variable
   `innodb_expand_import <http://www.percona.com/doc/percona-server/5.5/management/innodb_expand_import.html#innodb_expand_import>`_
   should be used instead of `innodb_import_table_from_xtrabackup <http://www.percona.com/doc/percona-server/5.5/management/innodb_expand_import.html#innodb_import_table_from_xtrabackup>`_.

Exporting the Table
===================

This table should have been created in :term:`innodb_file_per_table` mode, so
after taking a backup as usual with :option:`xtrabackup --backup`, the
:term:`.ibd` file should exist in the target directory:

.. code-block:: bash

  $ find /data/backups/mysql/ -name export_test.*
  /data/backups/mysql/test/export_test.ibd

when you prepare the backup, add the extra parameter
:option:`xtrabackup --export` to the command. Here is an example:

.. code-block:: bash

  $ xtrabackup --prepare --export --target-dir=/data/backups/mysql/

.. note::

  If you're trying to restore :ref:`encrypted InnoDB tablespace
  <encrypted_innodb_tablespace_backups>` table you'll need to specify the
  keyring file as well:

  .. code-block:: bash

    xtrabackup --prepare --export --target-dir=/tmp/table \
    --keyring-file-data=/var/lib/mysql-keyring/keyring

Now you should see a :term:`.exp` file in the target directory:

.. code-block:: bash

  $ find /data/backups/mysql/ -name export_test.*
  /data/backups/mysql/test/export_test.exp
  /data/backups/mysql/test/export_test.ibd
  /data/backups/mysql/test/export_test.cfg

These three files are all you need to import the table into a server running
|Percona Server| with |XtraDB| or |MySQL| 5.7. In case server is using `InnoDB
Tablespace Encryption
<http://dev.mysql.com/doc/refman/5.7/en/innodb-tablespace-encryption.html>`_
additional :file:`.cfp` file be listed for encrypted tables.

.. note::

  |MySQL| uses :file:`.cfg` file which contains |InnoDB| dictionary dump in
  special format. This format is different from the :file:`.exp`` one which is
  used in |XtraDB| for the same purpose. Strictly speaking, a :file:`.cfg``
  file is not required to import a tablespace to |MySQL| 5.7 or |Percona
  Server| 5.7. A tablespace will be imported successfully even if it is from
  another server, but |InnoDB| will do schema validation if the corresponding
  :file:`.cfg` file is present in the same directory.

Importing the Table
===================

On the destination server running |Percona Server| with |XtraDB| and
`innodb_import_table_from_xtrabackup <http://www.percona.com/doc/percona-server/5.5/management/innodb_expand_import.html#innodb_import_table_from_xtrabackup>`_
option enabled, or |MySQL| 5.6, create a table with the same structure, and
then perform the following steps:

* Execute ``ALTER TABLE test.export_test DISCARD TABLESPACE;``

  * If you see the following message, then you must enable
    :term:`innodb_file_per_table` and create the table again: ``ERROR 1030
    (HY000): Got error -1 from storage engine``

* Copy the exported files to the ``test/`` subdirectory of the destination
  server's data directory

* Execute ``ALTER TABLE test.export_test IMPORT TABLESPACE;``

The table should now be imported, and you should be able to ``SELECT`` from it
and see the imported data.

.. note::

  Persistent statistics for imported tablespace will be empty until you run the
  ``ANALYZE TABLE`` on the imported table. They will be empty because they are
  stored in the system tables ``mysql.innodb_table_stats`` and
  ``mysql.innodb_index_stats`` and they aren't updated by server during the
  import. This is due to upstream bug :mysqlbug:`72368`.
