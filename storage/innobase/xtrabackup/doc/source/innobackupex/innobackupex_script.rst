================================================================================
 The innobackupex Program
================================================================================

The |innobackupex| program is a symlink to the :ref:`xtrabackup
<xtrabackup-binary>` *C* program. It lets you perform
point-in-time backups of |InnoDB| / |XtraDB| tables together with the schema
definitions, |MyISAM| tables, and other portions of the server. In previous
versions |innobackupex| was implemented as a *Perl* script.

This manual section explains how to use |innobackupex| in detail.

.. warning::
   
   The |innobackupex| program is deprecated. Please switch to |xtrabackup|.

The Backup Cycle - Full Backups
================================================================================

.. toctree::
   :maxdepth: 1

   creating_a_backup_ibk
   preparing_a_backup_ibk
   restoring_a_backup_ibk

Other Types of Backup
================================================================================

.. toctree::
   :maxdepth: 1

   incremental_backups_innobackupex
   partial_backups_innobackupex
   encrypted_backups_innobackupex

Advanced Features
================================================================================

.. toctree::
   :maxdepth: 1

   streaming_backups_innobackupex
   replication_ibk
   parallel_copy_ibk
   throttling_ibk
   restoring_individual_tables_ibk
   pit_recovery_ibk
   improved_ftwrl
   storing_history


Implementation
================================================================================

.. toctree::
   :maxdepth: 1

   how_innobackupex_works


References
================================================================================

.. toctree::
   :maxdepth: 1

   innobackupex_option_reference

