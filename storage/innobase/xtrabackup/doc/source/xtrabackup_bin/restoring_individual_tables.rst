.. _export_import_tables:
.. _restoring_individual_tables:
.. _pxb.xtrabackup.table.restoring:

=============================
 Restoring Individual Tables
=============================

With |Percona XtraBackup|, you can export individual tables from any |InnoDB|
database, and import them into |Percona Server| with |XtraDB| or |MySQL| 8.0.
(The source doesn't have to be |XtraDB| or |MySQL| 8.0, but the destination
does.) This only works on individual :term:`.ibd` files, and cannot export a
table that is not contained in its own :term:`.ibd` file.

Let's see how to export and import the following table:

.. code-block:: mysql

   CREATE TABLE export_test (
   a int(11) DEFAULT NULL
   ) ENGINE=InnoDB DEFAULT CHARSET=latin1;

Exporting the Table
================================================================================

This table should be created in :term:`innodb_file_per_table` mode, so
after taking a backup as usual with :option:`--backup`, the
:term:`.ibd` file should exist in the target directory:

.. code-block:: bash

   $ find /data/backups/mysql/ -name export_test.*
   /data/backups/mysql/test/export_test.ibd

when you prepare the backup, add the extra parameter :option:`--export` to the
command. Here is an example:

.. code-block:: bash

   $ xtrabackup --prepare --export --target-dir=/data/backups/mysql/

.. note::

   If you're trying to restore :ref:`encrypted InnoDB tablespace
   <encrypted_innodb_tablespace_backups>` table you'll need to specify the
   keyring file as well:

   .. code-block:: bash

      $ xtrabackup --prepare --export --target-dir=/tmp/table \
      --keyring-file-data=/var/lib/mysql-keyring/keyring

Now you should see a :term:`.exp` file in the target directory:

.. code-block:: bash

   $ find /data/backups/mysql/ -name export_test.*
   /data/backups/mysql/test/export_test.exp
   /data/backups/mysql/test/export_test.ibd
   /data/backups/mysql/test/export_test.cfg

These three files are all you need to import the table into a server running
|Percona Server| with |XtraDB| or |MySQL| 8.0. In case server is using `InnoDB
Tablespace Encryption
<http://dev.mysql.com/doc/refman/5.7/en/innodb-tablespace-encryption.html>`_
additional :file:`.cfp` file be listed for encrypted tables.

.. note::

   |MySQL| uses :file:`.cfg` file which contains |InnoDB| dictionary dump in
   special format. This format is different from the :file:`.exp`` one which is
   used in |XtraDB| for the same purpose. Strictly speaking, a :file:`.cfg``
   file is not required to import a tablespace to |MySQL| 8.0 or |Percona
   Server| 8.0. A tablespace will be imported successfully even if it is from
   another server, but |InnoDB| will do schema validation if the corresponding
   :file:`.cfg` file is present in the same directory.

Importing the Table
================================================================================

On the destination server running |Percona Server| with |XtraDB| and
`innodb_import_table_from_xtrabackup
<http://www.percona.com/doc/percona-server/8.0/management/innodb_expand_import.html#innodb_import_table_from_xtrabackup>`_
option enabled, or |MySQL| 8.0, create a table with the same
structure, and then perform the following steps:

#. Run the :mysql:`ALTER TABLE test.export_test DISCARD TABLESPACE;`
   command. If you see this error then you must enable
   :term:`innodb_file_per_table` and create the table again.

   .. admonition:: Error

      ERROR 1030 (HY000): Got error -1 from storage engine

#. Copy the exported files to the :dir:`test/` subdirectory of the destination server's data directory

#. Run :mysql:`ALTER TABLE test.export_test IMPORT TABLESPACE;`

The table should now be imported, and you should be able to ``SELECT`` from it
and see the imported data.
