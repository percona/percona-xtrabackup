.. _backup_verification:

====================================================
 Verifying Backups with replication and pt-checksum
====================================================

One way to verify if the backup is consistent is by setting up the replication and running `pt-table-checksum <http://www.percona.com/doc/percona-toolkit/pt-table-checksum.html>`_. This can be used to verify any type of backups, but before setting up replication, backup should be prepared and be able to run (this means that incremental backups should be merged to full backups, encrypted backups decrypted etc.).

Setting up the replication
============================

:ref:`replication_howto` guide provides a detailed instructions on how to take the backup and set up the replication. 

For checking the backup consistency you can use either the original server where the backup was taken, or another test server created by using a different backup method (such as cold backup, mysqldump or LVM snapshots) as the source server in the replication setup.

Using pt-table-checksum
=========================

This tool is part of the *Percona Toolkit*. It performs an online replication consistency check by executing checksum queries on the source, which produces different results on replicas that are inconsistent with the source.

After you confirmed that replication has been set up successfully, you can `install <http://www.percona.com/doc/percona-toolkit/installation.html>`_ or download *pt-table-checksum*. This example shows downloading the latest version of *pt-table-checksum*: :: 

  $ wget percona.com/get/pt-table-checksum

.. note:: 

  In order for pt-table-checksum to work correctly ``libdbd-mysql-perl`` will need to be installed on *Debian/Ubuntu* systems or ``perl-DBD-MySQL`` on *RHEL/CentOS*. If you installed the *percona-toolkit* package from the Percona repositories package manager should install those libraries automatically.
 
After this command has been run, *pt-table-checksum* will be downloaded to your current working directory.

Running the *pt-table-checksum* on the source will create ``percona`` database with the ``checksums`` table which will be replicated to the replicas as well. Example of the *pt-table-checksum* will look like this: ::
 
    $ ./pt-table-checksum 
	TS ERRORS  DIFFS     ROWS  CHUNKS SKIPPED    TIME TABLE
	04-30T11:31:50      0      0   633135       8       0   5.400 exampledb.aka_name
    04-30T11:31:52      0      0   290859       1       0   2.692 exampledb.aka_title
    Checksumming exampledb.user_info:  16% 02:27 remain
    Checksumming exampledb.user_info:  34% 01:58 remain
    Checksumming exampledb.user_info:  50% 01:29 remain
    Checksumming exampledb.user_info:  68% 00:56 remain
    Checksumming exampledb.user_info:  86% 00:24 remain
    04-30T11:34:38      0      0 22187768     126       0 165.216 exampledb.user_info
    04-30T11:38:09      0      0        0       1       0   0.033 mysql.time_zone_name
    04-30T11:38:09      0      0        0       1       0   0.052 mysql.time_zone_transition
    04-30T11:38:09      0      0        0       1       0   0.054 mysql.time_zone_transition_type
    04-30T11:38:09      0      0        8       1       0   0.064 mysql.user

If all the values in the ``DIFFS`` column are 0 that means that backup is consistent with the current setup.

In case backup wasn't consistent  *pt-table-checksum* should spot the difference and point to the table that doesn't match. Following example shows adding new user on the backed up replica in order to simulate the inconsistent backup: ::

  mysql> grant usage on exampledb.* to exampledb@localhost identified by 'thisisnewpassword';

If we run the *pt-table-checksum* now difference should be spotted :: 

    $ ./pt-table-checksum 
    TS ERRORS  DIFFS     ROWS  CHUNKS SKIPPED    TIME TABLE
    04-30T11:31:50      0      0   633135       8       0   5.400 exampledb.aka_name
    04-30T11:31:52      0      0   290859       1       0   2.692 exampledb.aka_title
    Checksumming exampledb.user_info:  16% 02:27 remain
    Checksumming exampledb.user_info:  34% 01:58 remain
    Checksumming exampledb.user_info:  50% 01:29 remain
    Checksumming exampledb.user_info:  68% 00:56 remain
    Checksumming exampledb.user_info:  86% 00:24 remain
    04-30T11:34:38      0      0 22187768     126       0 165.216 exampledb.user_info
    04-30T11:38:09      0      0        0       1       0   0.033 mysql.time_zone_name
    04-30T11:38:09      0      0        0       1       0   0.052 mysql.time_zone_transition
    04-30T11:38:09      0      0        0       1       0   0.054 mysql.time_zone_transition_type
    04-30T11:38:09      1      0        8       1       0   0.064 mysql.user

This output shows that source and the replica aren't in consistent state and that the difference is in the ``mysql.user`` table.

More information on different options that pt-table-checksum provides can be found in the *pt-table-checksum* `documentation <http://www.percona.com/doc/percona-toolkit/2.2/pt-table-checksum.html>`_.
