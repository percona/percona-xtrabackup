# How-tos and Recipes

## Recipes for innobackupex

* [Make a Local Full Backup (Create, Prepare and Restore)](howtos/recipes_ibkx_local.md)

* [Make a Streaming Backup](howtos/recipes_ibkx_stream.md)

* [Making an Incremental Backup](howtos/recipes_ibkx_inc.md)

* [Making a Compressed Backup](howtos/recipes_ibkx_compressed.md)

* [Backing Up and Restoring Individual Partitions](howtos/recipes_ibkx_partition.md)

## Recipes for *xtrabackup*

* [Making a Full Backup](howtos/recipes_xbk_full.md)

* [Making an Incremental Backup](howtos/recipes_xbk_inc.md)

* [Restoring the Backup](howtos/recipes_xbk_restore.md)

## How-Tos

* [How to setup a replica for replication in 6 simple steps with Percona XtraBackup](howtos/setting_up_replication.md)

* [Verifying Backups with replication and pt-checksum](howtos/backup_verification.md)

* [How to create a new (or repair a broken) GTID based slave](howtos/recipes_ibkx_gtid.md)

## Auxiliary Guides

* [Enabling the server to communicate via TCP/IP](howtos/enabling_tcp.md)

* [Privileges and Permissions for Users](howtos/permissions.md)

* [Installing and configuring a SSH server](howtos/ssh_server.md)

## Assumptions in this section

The context should make the recipe or tutorial understandable. To assure that this is true, a list of the assumptions, names and other objects that appears in this section. This items are specified at the beginning of each recipe or tutorial.

`HOST`

A system with a *MySQL*-based server installed, configured and running. We assume the following about this system:

* The MySQL server is able to [communicate with others by the  standard TCP/IP port](howtos/enabling_tcp.md);

* An SSH server is installed and configured - see [here](howtos/ssh_server.md) if it is not;

* You have an user account in the system with the appropriate [permissions](howtos/permissions.md)

* You have a MySQL’s user account with appropriate [Connection and Privileges Needed](using_xtrabackup/privileges.md#privileges).

`USER`

This is a user account with shell access and the appropriate permissions for the task. A guide for checking them is [here](howtos/permissions.md).

`DB-USER`

This is a user account in the database server with the appropriate privileges for the task. A guide for checking them is [here](howtos/permissions.md).
