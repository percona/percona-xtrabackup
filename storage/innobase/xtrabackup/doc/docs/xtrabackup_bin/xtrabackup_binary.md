# The xtrabackup Binary

The *xtrabackup* binary is a compiled C program that is linked with the *InnoDB*
libraries and the standard *MySQL* client libraries. The *InnoDB* libraries
provide functionality necessary to apply a log to data files, and the *MySQL*
client libraries provide command-line option parsing, configuration file
parsing, and so on to give the binary a familiar look and feel.

The tool runs in either `xtrabackup --backup` or
`xtrabackup --prepare` mode, corresponding to the two main
functions it performs. There are several variations on these functions
to accomplish different tasks, and there are two less commonly used
modes, `xtrabackup --stats` and `xtrabackup --print-param`.

## Other Types of Backups

* [Incremental Backups](incremental_backups.md)

* [Partial Backups](partial_backups.md)

## Advanced Features

* [Throttling Backups](../advanced/throttling_backups.md)

* [Scripting Backups With xtrabackup](scripting_backups_xbk.md)

* [Analyzing Table Statistics](analyzing_table_statistics.md)

* [Working with Binary Logs](working_with_binary_logs.md)

* [Restoring Individual Tables](restoring_individual_tables.md)

* [LRU dump backup](lru_dump.md)

## Implementation

* [Implementation Details](implementation_details.md)

* [*xtrabackup* Exit Codes](xtrabackup_exit_codes.md)

## References

* [The xtrabackup Option Reference](xbk_option_reference.md)
