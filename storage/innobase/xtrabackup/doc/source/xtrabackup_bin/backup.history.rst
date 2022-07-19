.. _pxb.xtrabackup.history.storing:

================================================================================
Store backup history on the server
================================================================================
 
*Percona XtraBackup* supports storing the backups history on the server. This
feature was implemented in *Percona XtraBackup* 2.2. Storing backup history on
the server was implemented to provide users with additional information about
backups that are being taken. Backup history information will be stored in the
:ref:`PERCONA_SCHEMA.XTRABACKUP_HISTORY <xtrabackup_history>` table.

To use this feature the following options are available:

* :option:`--history` =<name> : This option enables the history
  feature and allows the user to specify a backup series name that will be
  placed within the history record.
* :option:`--incremental-history-name` =<name> : This option allows an
  incremental backup to be made based on a specific history series by
  name. *xtrabackup* will search the history table looking for the most recent
  (highest ``to_lsn``) backup in the series and take the ``to_lsn`` value to use
  as it's starting lsn. This is mutually exclusive with
  :option:`--incremental-history-uuid`, :option:`--incremental-basedir` and
  :option:`--incremental-lsn` options. If no valid LSN can be found
  (no series by that name) *xtrabackup* will return with an error.
* :option:`--incremental-history-uuid` =<uuid> : Allows an incremental backup to
  be made based on a specific history record identified by UUID. *xtrabackup*
  will search the history table looking for the record matching UUID and take
  the ``to_lsn`` value to use as it's starting LSN. This options is mutually
  exclusive with :option:`--incremental-basedir`, :option:`--incremental-lsn`
  and :option:`--incremental-history-name` options. If no valid LSN can be found
  (no record by that UUID or missing ``to_lsn``), *xtrabackup* will return
  with an error.

.. note::

   Backup that's currently being performed will **NOT** exist in the
   xtrabackup_history table within the resulting backup set as the record will
   not be added to that table until after the backup has been taken.

If you want access to backup history outside of your backup set in the case of
some catastrophic event, you will need to either perform a ``mysqldump``,
partial backup or ``SELECT`` * on the history table after *xtrabackup*
completes and store the results with you backup set.

For the necessary privileges, see :ref:`pxb.privilege`.

.. _xtrabackup_history:

.. rubric:: PERCONA_SCHEMA.XTRABACKUP_HISTORY table


This table contains the information about the previous server
backups. Information about the backups will only be written if the backup was
taken with :option:`--history` option.

.. list-table::
   :header-rows: 1
   :widths: 20 40
  
   * - Column Name
     - Description
   * - uuid 
     - Unique backup id
   * - name
     - User provided name of backup series. There may be multiple entries with the same name used to identify related backups in a series.
   * - tool_name
     - Name of tool used to take backup
   * - tool_command
     - Exact command line given to the tool with --password and --encryption_key obfuscated 
   * - tool_version
     - Version of tool used to take backup
   * - ibbackup_version
     - Version of the xtrabackup binary used to take backup
   * - server_version
     - Server version on which backup was taken
   * - start_time
     - Time at the start of the backup
   * - end_time
     - Time at the end of the backup
   * - lock_time
     - Amount of time, in seconds, spent calling and holding locks for ``FLUSH TABLES WITH READ LOCK``
   * - binlog_pos
     - Binlog file and position at end of ``FLUSH TABLES WITH READ LOCK``
   * - innodb_from_lsn
     - LSN at beginning of backup which can be used to determine prior backups
   * - innodb_to_lsn
     - LSN at end of backup which can be used as the starting lsn for the next incremental
   * - partial
     - Is this a partial backup, if ``N`` that means that it's the full backup
   * - incremental
     - Is this an incremental backup
   * - format
     - Description of result format (``xbstream``)
   * - compact
     - Is this a compact backup
   * - compressed
     - Is this a compressed backup
   * - encrypted
     - Is this an encrypted backup
 
.. rubric:: Limitations

* :option:`--history` option must be specified only on the command
  line and not within a configuration file in order to be effective.
* :option:`--incremental-history-name` and :option:`--incremental-history-uuid`
  options must be specified only on the command line and not within
  a configuration file in order to be effective.
