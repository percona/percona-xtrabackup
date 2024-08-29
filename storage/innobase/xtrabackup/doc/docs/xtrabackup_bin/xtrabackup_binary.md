# The xtrabackup Binary

The *xtrabackup* binary is a compiled C program that is linked with the *InnoDB*
libraries and the standard *MySQL* client libraries.

*xtrabackup* enables point-in-time backups of *InnoDB* / *XtraDB* tables
together with the schema definitions, *MyISAM* tables, and other portions of the
server.

The *InnoDB* libraries provide the functionality to apply a log to data
files. The *MySQL* client libraries are used to parse command-line options and
configuration file.

The tool runs in either `--backup` or `--prepare` mode,
corresponding to the two main functions it performs. There are several
variations on these functions to accomplish different tasks, and there are two
less commonly used modes, `--stats` and `--print-param`.

## Other Types of Backups


* Incremental Backups


* Partial Backups


## Advanced Features

<!-- NB: the following section has been removed because it is a
duplicate of a section in source/advanced:

throttling_backups -->

* Analyzing Table Statistics


* Working with Binary Logs


* Restoring Individual Tables


* LRU dump backup


* Streaming Backups


* Encrypting Backups


* `FLUSH TABLES WITH READ LOCK` option


* Accelerating the backup process


* Point-In-Time recovery


* Making Backups in Replication Environments


* Store backup history on the server


## Implementation


* Implementation Details


* *xtrabackup* Exit Codes


## References


* The **xtrabackup** Option Reference
