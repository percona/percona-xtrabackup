# How-tos and Recipes

## Recipes for *xtrabackup*


* [Making a Full Backup](https://docs.percona.com/percona-xtrabackup/8.0/backup_scenarios/full_backup.html)


* [Making an Incremental Backup](https://docs.percona.com/percona-xtrabackup/8.0/backup_scenarios/incremental_backup.html)


* [Make a Streaming Backup
](https://docs.percona.com/percona-xtrabackup/8.0/howtos/recipes_xbk_stream.html)

* [Making a Compressed Backup](https://docs.percona.com/percona-xtrabackup/8.0/backup_scenarios/compressed_backup.html)


* [Backing Up and Restoring Individual Partitions](https://docs.percona.com/percona-xtrabackup/8.0/howtos/recipes_xbk_partition.html)


## How-Tos


* [Privileges and Permissions for Users](https://docs.percona.com/percona-xtrabackup/8.0/howtos/permissions.html)


* [How to set up a replica for replication in 6 simple steps with Percona XtraBackup](https://docs.percona.com/percona-xtrabackup/8.0/howtos/setting_up_replication.html)


* [Verifying Backups with replication and pt-checksum](https://docs.percona.com/percona-xtrabackup/8.0/howtos/backup_verification.html)


* [How to create a new (or repair a broken) GTID-based Replica](https://docs.percona.com/percona-xtrabackup/8.0/howtos/recipes_ibkx_gtid.html)


## Assumptions in this section

Most of the time, the context will make the recipe or tutorial understandable.
To assure that, a list of the assumptions, names and “things” that will appear
in this section is given. At the beginning of each recipe or tutorial they will
be specified in order to make it quicker and more practical.

`HOST`

> A system with a *MySQL*-based server installed, configured and running. We
> will assume the following about this system:

> > 
> > * the MySQL server is able to communicate with others by the
> > standard TCP/IP port;


> > * an SSH server is installed and configured - see here if it is not;


> > * you have a user account in the system with the appropriate
> > permissions and


> > * you have a MySQL’s user account with appropriate Connection and Privileges Needed.

`USER`

    An user account in the system with shell access and appropriate permissions
    for the task. A guide for checking them is here.

`DB-USER`

    An user account in the database server with appropriate privileges for the
    task. A guide for checking them is here.
