.. _xtrabackup-binary:

================================================================================
 The xtrabackup Binary
================================================================================

The *xtrabackup* binary is a compiled C program that is linked with the *InnoDB*
libraries and the standard *MySQL* client libraries.

*xtrabackup* enables point-in-time backups of *InnoDB* / *XtraDB* tables
together with the schema definitions, *MyISAM* tables, and other portions of the
server.

The *InnoDB* libraries provide the functionality to apply a log to data
files. The *MySQL* client libraries are used to parse command-line options and
configuration file.

The tool runs in either :option:`--backup` or :option:`--prepare` mode,
corresponding to the two main functions it performs. There are several
variations on these functions to accomplish different tasks, and there are two
less commonly used modes, :option:`--stats` and :option:`--print-param`.

Other Types of Backups
================================================================================

.. toctree::
   :maxdepth: 1

   incremental_backups
   partial_backups

Advanced Features
================================================================================

.. NB: the following section has been removed because it is a
   duplicate of a section in source/advanced:

   throttling_backups

.. toctree::
   :maxdepth: 1

   analyzing_table_statistics
   working_with_binary_logs
   restoring_individual_tables
   lru_dump
   backup.streaming
   backup.encrypting
   flush-tables-with-read-lock
   backup.accelerating
   point-in-time-recovery
   replication
   backup.history
   
Implementation
================================================================================

.. toctree::
   :maxdepth: 1

   implementation_details
   xtrabackup_exit_codes

References
================================================================================

.. toctree::
   :maxdepth: 1

   xbk_option_reference
