==========
 Glossary
==========

.. glossary::

The following are a list of common terms:

.ARM
  Each table with the `Archive Storage Engine` has ``.ARM`` file which contains the metadata of it.

.ARZ
  Each table with the `Archive Storage Engine` has ``.ARZ`` file which contains the data of it.

.CSM
  Each table with the `CSV Storage Engine` has ``.CSM`` file which contains the metadata of it.

.CSV
  Each table with the `CSV Storage` engine has ``.CSV`` file which contains the data of it (which is a standard Comma Separated Value file).

.exp
  Files with the ``.exp`` extension are created by *Percona XtraBackup* per each *InnoDB* tablespace when the `xtrabackup --export` option is used on prepare. These files can be used to import those tablespaces on *Percona Server for MySQL* 5.5 or lower versions, see :doc:`restoring individual tables <xtrabackup_bin/restoring_individual_tables>`".

.frm
  For each table, the server creates a file with the ``.frm`` extension which contains the table definition (for all storage engines).

.ibd
  On a multiple tablespace setup (`innodb_file_per_table` enabled), *MySQL* will store each newly created table on a file with a ``.ibd`` extension.

.MYD
  Each *MyISAM* table has ``.MYD`` (MYData) file which contains the data.

.MYI
  Each *MyISAM* table has ``.MYI`` (MYIndex) file which contains the table's indexes.

.MRG
  Each table using the `MERGE` storage engine, besides of an `.frm` file will have `.MRG` file containing the names of the *MyISAM* tables associated with it.

.opt
  *MySQL* stores options of a database (like charset) in a file with an `.opt` extension in the database directory.

.par
  Each partitioned table has .par file which contains metadata about the partitions.

.TRG
  File containing the Triggers associated to a table, for example,
  ``mytable.TRG`. With the `.TRN` file, they represent all the
  Trigger definitions.

.TRN
  File containing the Triggers' Names associated to a table, for example,
  ``mytable.TRN`. With the `.TRG` file, they represent all the
  Trigger definitions.

datadir
  The directory in which the database server stores its databases. Most Linux distributions use the `/var/lib/mysql` location by default.

  ibdata
   Default prefix for tablespace files, for example, `ibdata1` is a 10MB
   auto-extensible file that *MySQL* creates for the shared tablespace by
   default.

InnoDB
  Storage engine which provides ACID-compliant transactions and foreign key support, among others improvements over `MyISAM`. It is the default engine for *MySQL* as of the 5.5 series.

innodb_buffer_pool_size
  The size in bytes of the memory buffer to cache data and indexes of the *InnoDB*'s tables. This aims to reduce disk access to provide better performance. By default:

  .. code-block:: text

    [mysqld]
    innodb_buffer_pool_size=8MB

innodb_data_file_path
  Specifies the names, sizes and location of shared tablespace files:

  .. code-block:: text

     [mysqld]
     innodb_data_file_path=ibdata1:50M;ibdata2:50M:autoextend


innodb_data_home_dir
  The directory (relative to `datadir`) where the database server stores the files in a shared tablespace setup. This option does not affect the location of `innodb_file_per_table`. For example:

  .. code-block:: text

      [mysqld]
      innodb_data_home_dir = ./

innodb_expand_import
  This feature of *Percona Server for MySQL* implements the ability to import arbitrary `.ibd` files exported using the *Percona XtraBackup* `xtrabackup --export` option.

innodb_file_per_table
 By default, InnoDB creates tables and indexes in a `file-per-tablespace <https://dev.mysql.com/doc/refman/8.0/en/innodb-file-per-table-tablespaces.html>`__. If the ``innodb_file_per_table`` variable is disabled, you can start the server with ``--innodb_file_per_table`` or enable the variable in your configuration file:

 .. code-block:: text

    [mysqld]
    innodb_file_per_table

innodb_log_group_home_dir
  Specifies the location of the *InnoDB* log files:

  .. code-block:: text

      [mysqld]
      innodb_log_group_home=/var/lib/mysql

LSN
 Each InnoDB page (usually 16kb in size) contains a log sequence number (LSN). The LSN is the system version number for the entire database. Each
 page's LSN shows how recently it was changed.

MyISAM
 Previous default storage engine for *MySQL* for versions prior to 5.5. It
 does not fully support transactions but in some scenarios may be faster
 than `InnoDB`. Each table is stored on disk in 3 files:
 `.frm`, `.MYD`, `.MYI`.


my.cnf
  This file refers to the database server's main configuration file. Most Linux distributions have this file at `/etc/mysql/my.cnf` or `/etc/my.cnf`, but the location and name depends on the particular installation. Note that this is not the only way to configure the server, and some systems rely only on the command options to start the server and set the defaults values.

UUID
  A Universally Unique Identifier (UUID) which uniquely identifies the state and the sequence of changes on a node. The 128-bit UUID is a classic DCE UUID Version 1 and is based on current time and MAC address. In Galera, it is based on the generated pseudo-random addresses ("locally administered" bit in the node address (in the UUID structure) is always equal to unity).

XtraDB
  *Percona XtraDB* is an enhanced version of the InnoDB storage engine,
  designed to better scale on modern hardware, and includes a variety of
  other features useful in high performance environments. It is fully
  backwards compatible, and so can be used as a drop-in replacement for
  standard InnoDB. More information `here
  <https://www.percona.com/doc/percona-server/5.7/percona_xtradb.html>`_.

xbcrypt
  To support encryption and decryption of the backups, the xbcrypt utility was added to *Percona XtraBackup*. This utility has been modeled after the xbstream binary to perform encryption and decryption outside of *Percona XtraBackup*.

xbstream
  To support simultaneous compression and streaming, a new custom streaming format called xbstream was introduced to *Percona XtraBackup*.
