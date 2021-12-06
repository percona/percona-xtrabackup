.. _comparison:

Compare Server Version to the Backup Version 
=======================================================

*Percona XtraBackup* 8.0.21 adds the ``--no-server-version-check`` option. 

Before the backup starts, XtraBackup compares the source system version to the *Percona XtraBackup* version. If the source system version is greater than the XtraBackup version, XtraBackup stops the backup and returns an error message. This comparison prevents a failed backup or a corrupted backup due to source system changes. 

This option overrides the comparison.

The backup proceeds in the following comparison scenarios:

* The source system version and the XtraBackup version are the same

* The source system version is less than the XtraBackup version

* The source system version is greater than the XtraBackup version, and the option is added to the command

If the source system is *MySQL* 8.0.27 and the XtraBackup is 8.0.23-16.0, add the option, as in the example, and the backup proceeds.

.. code-block:: bash

    $ xtrabackup --backup --no-server-version-check --target-dir=$mysql/backup1

If you override the comparison, the following events can happen:

* Backup fails

* Backup is corrupted

* Backup successful



