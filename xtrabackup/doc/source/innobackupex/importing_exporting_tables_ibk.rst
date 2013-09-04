.. _imp_exp_ibk:

===========================================
 Importing and Exporting Individual Tables
===========================================

In standard |InnoDB|, it is not normally possible to copy tables between servers by copying the files, even with :term:`innodb_file_per_table` enabled. But |XtraBackup| allows to migrate individual table from any |InnoDB| database to |Percona Server| with |XtraDB|.

The table is required to be created with the option :term:`innodb_file_per_table` enabled in the server, as exporting is only possible when table is stored in its own table space.

The importing server (at the moment it only supported by |Percona Server|) should have :term:`innodb_file_per_table` and :term:`innodb_expand_import` options enabled.

Exporting tables
================

Exporting is done in the preparation stage, not at the moment of creating the backup. Once a full backup is created, prepare it with the :option:`--export` option: ::

  $ innobackupex --apply-log --export /path/to/backup

This will create for each |InnoDB| with its own tablespace a file with :term:`.exp` extension. An output of this procedure would contain: ::

  ..
  xtrabackup: export option is specified.
  xtrabackup: export metadata of table 'mydatabase/mytable' to file
  `./mydatabase/mytable.exp` (1 indexes)
  ..

Each :term:`.exp` file will be used for importing that table.

.. note::

  InnoDB does a slow shutdown (i.e. full purge + change buffer merge) on --export, otherwise the tablespaces wouldn't be consistent and thus couldn't be imported. All the usual performance considerations apply: sufficient buffer pool (i.e. --use-memory, 100MB by default) and fast enough storage, otherwise it can take a prohibitive amount of time for export to complete.

Importing tables
================

To import a table to other server, first create a new table with the same structure as the one that will be imported at that server: ::

  OTHERSERVER|mysql> CREATE TABLE mytable (...) ENGINE=InnoDB;

then discard its tablespace: ::

   OTHERSERVER|mysql> ALTER TABLE mydatabase.mytable DISCARD TABLESPACE;

After this, copy :file:`mytable.ibd` and :file:`mytable.exp` files to database's home, and import its tablespace: ::

   OTHERSERVER|mysql> ALTER TABLE mydatabase.mytable IMPORT TABLESPACE;

Once this is executed, data in the imported table will be available.

