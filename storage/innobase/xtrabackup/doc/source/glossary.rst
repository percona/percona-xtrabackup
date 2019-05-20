==========
 Glossary
==========

.. glossary::

   UUID
     Universally Unique IDentifier which uniquely identifies the state and the
     sequence of changes node undergoes. 128-bit UUID is a classic DCE UUID
     Version 1 (based on current time and MAC address). Although in theory this
     UUID could be generated based on the real MAC-address, in the Galera it is
     always (without exception) based on the generated pseudo-random addresses
     ("locally administered" bit in the node address (in the UUID structure) is
     always equal to unity).

     Complete structure of the 128-bit UUID field and explanation for its
     generation are as follows:

     ===== ====  ======= =====================================================
     From  To    Length  Content
     ===== ====  ======= =====================================================
      0     31    32     Bits 0-31 of Coordinated Universal Time (UTC) as a
                         count of 100-nanosecond intervals since 00:00:00.00,
                         15 October 1582, encoded as big-endian 32-bit number.
     32     47    16     Bits 32-47 of UTC as a count of 100-nanosecond
                         intervals since 00:00:00.00, 15 October 1582, encoded
                         as big-endian 16-bit number.
     48     59    12     Bits 48-59 of UTC as a count of 100-nanosecond
                         intervals since 00:00:00.00, 15 October 1582, encoded
                         as big-endian 16-bit number.
     60     63     4     UUID version number: always equal to 1 (DCE UUID).
     64     69     6     most-significants bits of random number, which
                         generated from the server process PID and Coordinated
                         Universal Time (UTC) as a count of 100-nanosecond
                         intervals since 00:00:00.00, 15 October 1582.
     70     71     2     UID variant: always equal to binary 10 (DCE variant).
     72     79     8     8 least-significant bits of  random number, which
                         generated from the server process PID and Coordinated
                         Universal Time (UTC) as a count of 100-nanosecond
                         intervals since 00:00:00.00, 15 October 1582.
     80     80     1     Random bit ("unique node identifier").
     81     81     1     Always equal to the one ("locally administered MAC
                         address").
     82    127    46     Random bits ("unique node identifier"): readed from
                         the :file:`/dev/urandom` or (if :file:`/dev/urandom`
                         is unavailable) generated based on the server process
                         PID, current time and bits of the default "zero node
                         identifier" (entropy data).
     ===== ====  ======= =====================================================

   LSN
     Each InnoDB page (usually 16kb in size) contains a log sequence number, or
     LSN. The LSN is the system version number for the entire database. Each
     page's LSN shows how recently it was changed.

   innodb_file_per_table
     By default, all InnoDB tables and indexes are stored in the system
     tablespace on one file. This option causes the server to create one
     tablespace file per table. To enable it, set it on your configuration file,

      .. code-block:: text

         [mysqld]
         innodb_file_per_table

      or start the server with ``--innodb_file_per_table``.

   innodb_expand_import
     This feature of |Percona Server| implements the ability to import
     arbitrary :term:`.ibd` files exported using the |Percona XtraBackup|
     :option:`xtrabackup --export` option.

     See the `the full documentation
     <http://www.percona.com/doc/percona-server/5.5/management/innodb_expand_import.html>`_
     for more information.

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

   innodb_log_group_home_dir
     Specifies the location of the |InnoDB| log files:

      .. code-block:: text

         [mysqld]
         innodb_log_group_home=/var/lib/mysql

   innodb_buffer_pool_size
     The size in bytes of the memory buffer to cache data and indexes of
     |InnoDB|'s tables. This aims to reduce disk access to provide better
     performance. By default:

      .. code-block:: text

         [mysqld]
         innodb_buffer_pool_size=8MB

   InnoDB
      Storage engine which provides ACID-compliant transactions and foreign
      key support, among others improvements over :term:`MyISAM`. It is the
      default engine for |MySQL| as of the 5.5 series.

   MyISAM
     Previous default storage engine for |MySQL| for versions prior to 5.5. It
     doesn't fully support transactions but in some scenarios may be faster
     than :term:`InnoDB`. Each table is stored on disk in 3 files:
     :term:`.frm`, :term:`.MYD`, :term:`.MYI`.

   XtraDB
     *Percona XtraDB* is an enhanced version of the InnoDB storage engine,
     designed to better scale on modern hardware, and including a variety of
     other features useful in high performance environments. It is fully
     backwards compatible, and so can be used as a drop-in replacement for
     standard InnoDB. More information `here
     <https://www.percona.com/doc/percona-server/5.6/percona_xtradb.html>`_.

   my.cnf
     This file refers to the database server's main configuration file. Most
     Linux distributions place it as :file:`/etc/mysql/my.cnf` or
     :file:`/etc/my.cnf`, but the location and name depends on the particular
     installation. Note that this is not the only way of configuring the
     server, some systems does not have one even and rely on the command
     options to start the server and its defaults values.

   datadir
    The directory in which the database server stores its databases. Most Linux
    distribution use :file:`/var/lib/mysql` by default.

   xbcrypt
     To support encryption and decryption of the backups, a new tool xbcrypt
     was introduced to |Percona XtraBackup|. This utility has been modeled
     after The xbstream binary to perform encryption and decryption outside of
     |Percona XtraBackup|.

   xbstream
     To support simultaneous compression and streaming, a new custom streaming
     format called xbstream was introduced to |Percona XtraBackup| in addition
     to the TAR format.

   ibdata
     Default prefix for tablespace files, e.g. :file:`ibdata1` is a 10MB
     auto-extensible file that |MySQL| creates for the shared tablespace by
     default.

   .frm
     For each table, the server will create a file with the ``.frm`` extension
     containing the table definition (for all storage engines).

   .ibd
     On a multiple tablespace setup (:term:`innodb_file_per_table` enabled),
     |MySQL| will store each newly created table on a file with a ``.ibd``
     extension.

   .MYD
     Each |MyISAM| table has ``.MYD`` (MYData) file which contains the data on
     it.

   .MYI
     Each |MyISAM| table has ``.MYI`` (MYIndex) file which contains the table's
     indexes.

   .exp
     Files with the ``.exp`` extension are created by |Percona XtraBackup| per
     each |InnoDB| tablespace when the :option:`xtrabackup --export` option is
     used on prepare. These files can be used to import those tablespaces on
     |Percona Server| 5.5 or lower versions, see :doc:`restoring individual
     tables <xtrabackup_bin/restoring_individual_tables>`".

   .MRG
     Each table using the :program:`MERGE` storage engine, besides of a
     :term:`.frm` file, will have :term:`.MRG` file containing the names of the
     |MyISAM| tables associated with it.

   .TRG
     File containing the Triggers associated to a table, e.g.
     `:file:`mytable.TRG`. With the :term:`.TRN` file, they represent all the
     Trigger definitions.

   .TRN
     File containing the Triggers' Names associated to a table, e.g.
     `:file:`mytable.TRN`. With the :term:`.TRG` file, they represent all the
     Trigger definitions.

   .ARM
     Each table with the :program:`Archive Storage Engine` has ``.ARM`` file
     which contains the metadata of it.

   .ARZ
     Each table with the :program:`Archive Storage Engine` has ``.ARZ`` file
     which contains the data of it.

   .CSM
     Each table with the :program:`CSV Storage Engine` has ``.CSM`` file which
     contains the metadata of it.

   .CSV
     Each table with the :program:`CSV Storage` engine has ``.CSV`` file which
     contains the data of it (which is a standard Comma Separated Value file).

   .opt
     |MySQL| stores options of a database (like charset) in a file with a
     :file:`.opt` extension in the database directory.

   .par
     Each partitioned table has .par file which contains metadata about the
     partitions.
