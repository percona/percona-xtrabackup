==========
 Glossary
==========

.. glossary::

   .ARM file
     Contains the metadata for each Archive Storage Engine table.

   .ARZ file
     Contains the data for each Archive Storage Engine table.

   .CSM
     Each table with the :program:`CSV Storage Engine` has ``.CSM`` file which
     contains the metadata of it.

   .CSV
     Each table with the :program:`CSV Storage` engine has ``.CSV`` file which
     contains the data of it (which is a standard Comma Separated Value file).

   .exp
     Files with the ``.exp`` extension are created by *Percona XtraBackup* per
     each *InnoDB* tablespace when the `--export` option is
     used in the prepare phase. See :doc:`restoring individual tables <xtrabackup_bin/restoring_individual_tables>`.

   .frm
     For each table, the server will create a file with the ``.frm`` extension
     containing the table definition (for all storage engines).

   .ibd
     On a multiple tablespace setup (:term:`innodb_file_per_table` enabled),
     *MySQL* will store each newly created table in a file with a ``.ibd``
     extension.

   .MRG
     Each table using the :program:`MERGE` storage engine, besides of a
     :term:`.frm` file, will have :term:`.MRG` file containing the names of the
     *MyISAM* tables associated with it.

   .MYD
     Each *MyISAM* table has ``.MYD`` (MYData) file which contains the data on
     it.

   .MYI
     Each *MyISAM* table has ``.MYI`` (MYIndex) file which contains the table's
     indexes.

   .opt
     *MySQL* stores options of a database (like charset) in a file with a
     :file:`.opt` extension in the database directory.

   .par
     Each partitioned table has .par file which contains metadata about the
     partitions.

   .TRG
     File containing the triggers associated with a table, for example
     `:file:`mytable.TRG`. With the :term:`.TRN` file, they represent all the
     Trigger definitions.

   .TRN
     File containing the names of the triggers that are associated with a table, for example
     `:file:`mytable.TRN`. With the :term:`.TRG` file, they represent all the
     trigger definitions.

   backup
      The process of copying data or tables to be stored in a different location. 

   compression
      The method that produces backups in a reduced size. 

   configuration file
      The file that contains the server startup options. 

   crash
      An unexpected shutdown that does not allow the normal server shutdown cleanup activities. 

   crash recovery
      The actions that occur when MySQL is restarted after a crash. 

   data dictionary
     The metadata for the tables, indexes, and table columns stored in the InnoDB system tablespace.
  
   datadir
    The directory in which the database server stores its data files. Most Linux
    distribution use `/var/lib/mysql` by default.

   full backup
     A backup that contains the complete source data from an instance. 

   ibdata
     Default prefix for tablespace files, for example, :file:`ibdata1` is a 10MB
     auto-extensible file that *MySQL* creates for the shared tablespace by
     default.

   incremental backup
      A backup stores data from a specific point in time.

   InnoDB
      Storage engine which provides ACID-compliant transactions and foreign
      key support, among others improvements over :term:`MyISAM`. It is the
      default engine for *MySQL* as of the 8.0 series.

   innodb_buffer_pool_size
     The size in bytes of the memory buffer to cache data and indexes of
     *InnoDB*'s tables. This aims to reduce disk access to provide better
     performance. By default:

      .. code-block:: text

         [mysqld]
         innodb_buffer_pool_size=8MB

   innodb_data_home_dir
     The directory (relative to :term:`datadir`) where the database server
     stores the files in a shared tablespace setup. This option does not affect
     the location of :term:`innodb_file_per_table`. For example:

      .. code-block:: text

         [mysqld]
         innodb_data_home_dir = ./

   innodb_data_file_path
     Specifies the names, sizes and location of shared tablespace files:

      .. code-block:: text

         [mysqld]
         innodb_data_file_path=ibdata1:50M;ibdata2:50M:autoextend

   innodb_file_per_table
     By default, InnoDB creates tables and indexes in a `file-per-tablespace <https://dev.mysql.com/doc/refman/8.0/en/innodb-file-per-table-tablespaces.html>`__. If the ``innodb_file_per_table`` variable is disabled, you can enable the variable in your configuration file:

      .. code-block:: text

         [mysqld]
         innodb_file_per_table

      or start the server with ``--innodb_file_per_table``.

   innodb_log_group_home_dir
     Specifies the location of the *InnoDB* log files:

      .. code-block:: text

         [mysqld]
         innodb_log_group_home=/var/lib/mysql

   logical backup
      A backup which contains a set of SQL statements. The statements can be used to recreate the databases. 

   LSN
     Each InnoDB page (usually 16kb in size) contains a log sequence number, or
     LSN. The LSN is the system version number for the entire database. Each
     page's LSN shows how recently it was changed.

   my.cnf
     The database server's main configuration file. Most
     Linux distributions place it as :file:`/etc/mysql/my.cnf` or
     :file:`/etc/my.cnf`, but the location and name depends on the particular
     installation. Note that this is not the only way of configuring the
     server, some systems does not have one and rely on the command
     options to start the server and its default values.

   MyISAM
     Previous default storage engine for *MySQL* for versions prior to 5.5. It
     doesn't fully support transactions but in some scenarios may be faster
     than :term:`InnoDB`. Each table is stored on disk in 3 files:
     :term:`.frm`, :term:`.MYD`, :term:`.MYI`.
  
   physical backup
     A backup that copies the data files.

   point in time recovery
     This method allows data to be restored to the state it was in any selected point of time.

   prepared backup
     A consistent set of backup data that is ready to be restored.
     
   restore
     Copies the database backups taken using the backup command to the original location or a different location. A restore returns data that has been either lost, corrupted, or stolen to the original condition at a specific point in time.
 
   xbcrypt
     To support encryption and decryption of the backups, a new tool xbcrypt
     was introduced to *Percona XtraBackup*. This utility has been modeled
     after the xbstream binary to perform encryption and decryption outside of
     *Percona XtraBackup*.

   xbstream
     To support simultaneous compression and streaming, *Percona XtraBackup* uses the xbstream format. For more information, see :option:`--stream` 

   XtraDB
     *Percona XtraDB* is an enhanced version of the InnoDB storage engine,
     designed to better scale on modern hardware. *Percona XtraDB* includes a variety of
     other features useful in high performance environments. It is fully
     backwards compatible, and so can be used as a drop-in replacement for
     standard InnoDB. More information `here
     <https://www.percona.com/doc/percona-server/8.0/percona_xtradb.html>`_.



