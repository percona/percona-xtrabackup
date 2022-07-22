# The innobackupex Program

The *innobackupex* program is a symlink to the [xtrabackup](../xtrabackup_bin/xtrabackup_binary.md#xtrabackup-binary) *C* program. It lets you perform
point-in-time backups of *InnoDB* / *XtraDB* tables together with the schema
definitions, *MyISAM* tables, and other portions of the server. In previous
versions *innobackupex* was implemented as a *Perl* script.

This manual section explains how to use *innobackupex* in detail.

!!! warning

    The *innobackupex* program is deprecated. Please switch to *xtrabackup*.

## The Backup Cycle - Full Backups

* [Creating a Backup with innobackupex](creating_a_backup_ibk.md)

* [Preparing a Full Backup with *innobackupex*](preparing_a_backup_ibk.md)

* [Restoring a Full Backup with *innobackupex*](restoring_a_backup_ibk.md)

## Other Types of Backup

* [Incremental Backups with *innobackupex*](incremental_backups_innobackupex.md)

* [Partial Backups](partial_backups_innobackupex.md)

* [Encrypted Backups](encrypted_backups_innobackupex.md)

## Advanced Features

* [Streaming and Compressing Backups](streaming_backups_innobackupex.md)

* [Taking Backups in Replication Environments](replication_ibk.md)

* [Accelerating the backup process](parallel_copy_ibk.md)

* [Throttling backups with *innobackupex*](throttling_ibk.md)

* [Restoring Individual Tables](restoring_individual_tables_ibk.md)

* [Point-In-Time recovery](pit_recovery_ibk.md)

* [Improved `FLUSH TABLES WITH READ LOCK` handling](improved_ftwrl.md)

* [Store backup history on the server](storing_history.md)

## Implementation

* [How innobackupex Works](how_innobackupex_works.md)

## References

* [The innobackupex Option Reference](innobackupex_option_reference.md)
