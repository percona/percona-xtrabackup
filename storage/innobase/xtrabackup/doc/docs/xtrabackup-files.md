# Index of files created by Percona XtraBackup


* Information related to the backup and the server

    * `backup-my.cnf`
     
      This file contains information to start the mini instance of InnoDB
      during the `xtrabackup --prepare`. This is **NOT** a backup of
      original `my.cnf`. The InnoDB configuration is read from the file
      `backup-my.cnf` created by innobackupex when the backup was
      made. `xtrabackup --prepare` uses InnoDB configuration from
      `backup-my.cnf` by default, or from
      `xtrabackup --defaults-file`, if specified. InnoDB
      configuration in this context means server variables that affect data
      format, i.e. `innodb_page_size` option,
      `innodb_log_block_size`, etc. Location-related variables, like
      `innodb_log_group_home_dir` or `innodb_data_file_path`
      are always ignored by `xtrabackup --prepare`, so preparing
      a backup always works with data files from the backup directory, rather
      than any external ones.
 
    * `xtrabackup_checkpoints`

       The type of the backup (e.g. full or incremental), its state (e.g.
       prepared) and the LSN range contained in it. This information is used
       for incremental backups.
       Example of the `xtrabackup_checkpoints` after taking a full
       backup:

       ```text
       backup_type = full-backuped
       from_lsn = 0
       to_lsn = 15188961605
       last_lsn = 15188961605
       ```
       Example of the `xtrabackup_checkpoints` after taking an incremental backup:

       ```text
       backup_type = incremental
       from_lsn = 15188961605
       to_lsn = 15189350111
       last_lsn = 15189350111
       ```

    * `xtrabackup_binlog_info`

      The binary log file used by the server and its position at the moment of
      the backup. Result of the `SHOW MASTER STATUS`.

    * `xtrabackup_binlog_pos_innodb`

      The binary log file and its current position for *InnoDB* or XtraDB
      tables.

    * `xtrabackup_binary`

      The *xtrabackup* binary used in the process.

    * `xtrabackup_logfile`

      Contains data needed for running the: `xtrabackup --prepare`.
      The bigger this file is the `xtrabackup --prepare` process
      will take longer to finish.

    * `<table_name>.delta.meta`

      This file is going to be created when performing the incremental backup.
      It contains the per-table delta metadata: page size, size of compressed
      page (if the value is 0 it means the tablespace isn’t compressed) and
      space id. Example of this file could looks like this:

      ```text
      page_size = 16384
      zip_size = 0
      space_id = 0
      ```

* Information related to the replication environment (if using the
`xtrabackup --slave-info` option):
     
    * `xtrabackup_slave_info`
  
      The `CHANGE MASTER` statement needed for setting up a slave.

* Information related to the *Galera* and *Percona XtraDB Cluster* (if using
the `xtrabackup --galera-info` option):
 
    * `xtrabackup_galera_info`

      Contains the values of `wsrep_local_state_uuid` and
      `wsrep_last_committed` status variables
