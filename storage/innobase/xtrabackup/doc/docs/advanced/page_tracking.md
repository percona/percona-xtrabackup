# Incremental backup using page tracking

This feature is *tech preview* quality.

Percona XtraBackup 8.0.27 adds support for the page tracking functionality
for [incremental backup](https://www.percona.com/doc/percona-xtrabackup/8.0/backup_scenarios/incremental_backup.html).

To create an incremental backup with page tracking, Percona XtraBackup uses
the MySQL mysqlbackup component. This component provides a list of pages
modified since the last backup, and Percona XtraBackup copies only those
pages. This operation removes the need to scan the pages in the
database. If the majority of pages have not been modified, the page
tracking feature can improve the speed of incremental backups.

## Install the component

To start using the page tracking functionality, do the following steps:

1. Install the mysqlbackup component and enable it on the server:

> ```
> $ INSTALL COMPONENT "file://component_mysqlbackup";
> ```

1. Check whether the mysqlbackup component is installed successfully:

> ```
> SELECT COUNT(1) FROM mysql.component WHERE component_urn='file://component_mysqlbackup';
> ```

## Using page tracking

You can enable the page tracking functionality for the full and incremental
backups with the `--page-tracking` option.

The option has the following benefits:

* Resets page tracking to the start of the backup. This reset allows the
  next incremental backup to use page tracking.


* Allows the use of page tracking for an incremental backup if the page
  tracking data is available from the backup’s start checkpoint LSN.

> **NOTE**: Percona XtraBackup processes a list of all the tracked pages in
> memory. If Percona XtraBackup does not have enough available memory to
> process this list, Percona XtraBackup throws an
> error and exits. For example, if an incremental backup uses 200GB, Percona
> XtraBackup can additionally use about 100MB of memory to store the page
> tracking data.

An example of creating a full backup using the `--page-tracking` option.

> ```
> $> xtrabackup --backup --target-dir=$FULL_BACK --page-tracking
> ```

An example of creating an incremental backup using the `--page-tracking`
option.

> ```
> $> xtrabackup --backup --target-dir=$INC_BACKUP  
> --incremental-basedir=$FULL_BACKUP --page-tracking
> ```

After enabling the functionality, the next incremental backup finds changed
pages using page tracking.

**NOTE**: For the very first full backup using page tracking, Percona
XtraBackup may have a delay. The following is an example of the message you
can receive:

```
xtrabackup: pagetracking: Sleeping for 1 second, waiting for checkpoint lsn 17852922 /
to reach to page tracking start lsn 21353759
```

You can avoid this delay by enabling page tracking before creating the
first backup. Thus, you ensure that page tracking log sequence number (LSN)
is more than the checkpoint LSN of the server.

## Start page tracking manually

After the mysqlbackup component is loaded and active on the server, you can
start page tracking manually with the following option:

> ```
> $ SELECT mysqlbackup_page_track_set(true);
> ```

## Check the LSN value

Check the LSN value starting from which changed pages are tracked with the
following option:

> ```
> $ SELECT mysqlbackup_page_track_get_start_lsn();
> ```

## Stop page tracking

To stop page tracking, use the following command:

> ```
> $ SELECT mysqlbackup_page_track_set(false);
> ```

## Purge page tracking data

When you start page tracking, it creates a file under the server’s datadir
to collect data about changed pages. This file grows until you stop the
page tracking. If you stop the server and then restart it, page tracking
creates a new file but also keeps the old one. The old file continues to
grow until you stop the page tracking explicitly.

If you purge the page tracking data, you should create a full backup
afterward. To purge the page tracking data, do the following steps:

> ```
> $ SELECT mysqlbackup_page_track_set(false);
> $ SELECT mysqlbackup_page_track_purge_up_to(9223372036854775807);
> /* Specify the LSN up to which you want to purge page tracking data. /
> 9223372036854775807 is the highest possible LSN which purges all page tracking files.*/
> $ SELECT mysqlbackup_page_track_set(true);
> ```

## Known issue

If the index is built in place using an exclusive algorithm and then is
added to a table after the last LSN checkpoint, you may get a bad
incremental backup using page tracking. Find more details
in [PS-8032](https://jira.percona.com/browse/PS-8032) .

## Uninstall the mysqlbackup component

If you need to uninstall the mysqlbackup component, use the following
option:

> ```
> $ UNINSTALL COMPONENT "file://component_mysqlbackup"
> ```
