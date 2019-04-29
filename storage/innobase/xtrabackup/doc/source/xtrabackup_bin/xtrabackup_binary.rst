.. _xtrabackup-binary:

================================================================================
 The xtrabackup Binary
================================================================================

The |xtrabackup| binary is a compiled C program that is linked with the |InnoDB|
libraries and the standard |MySQL| client libraries. The |InnoDB| libraries
provide functionality necessary to apply a log to data files, and the |MySQL|
client libraries provide command-line option parsing, configuration file
parsing, and so on to give the binary a familiar look and feel.

The tool runs in either :option:`xtrabackup --backup` or
:option:`xtrabackup --prepare` mode, corresponding to the two main
functions it performs. There are several variations on these functions
to accomplish different tasks, and there are two less commonly used
modes, :option:`xtrabackup --stats` and :option:`xtrabackup --print-param`.

Other Types of Backups
================================================================================

.. toctree::
   :maxdepth: 1

   incremental_backups
   partial_backups

Advanced Features
================================================================================

.. toctree::
   :maxdepth: 1

   ../advanced/throttling_backups
   scripting_backups_xbk
   analyzing_table_statistics
   working_with_binary_logs
   restoring_individual_tables
   lru_dump
   
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
