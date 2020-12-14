.. _comparison:

Server Version and Backup Version Comparison
=============================================

|PXB| 8.0.21 adds the ``--no-server-version-check`` parameter. This parameter compares the source system version to the |PXB| version. 

The parameter checks for the following scenarios:

* The source system and the PXB version are the same, the backup proceeds

* The source system is less than the PXB version, the backup proceeds

* The source system is greater than the PXB version, and the parameter is not overridden, the backup is stopped and returns an error message

* The source system is greater than the PXB version, and the parameter is overridden, the backup proceeds

Explicitly adding the ``--no-server-version-check`` parameter, like the example, overrides the parameter and the backup proceeds.

.. code-block:: bash

    $ xtrabackup --backup --no-server-version-check --target-dir=$mysql/backup1

When you override the parameter, the following events can happen:

* Backup fails

* Creates a corrupted backup

* Backup successful



