.. _xtrabackup.full:

================================================================================
 Making a Full Backup
================================================================================

Backup the InnoDB data and log files located in ``/var/lib/mysql/`` to
``/data/backups/mysql/`` (destination). Then, prepare the backup files to be
ready to restore or use (make the data files consistent).

.. rubric:: Making a backup

.. code-block:: bash

   $ xtrabackup --backup --target-dir=/data/backup/mysql/

.. rubric:: Preparing the backup twice

.. code-block:: bash

  $ xtrabackup --prepare --target-dir=/data/backup/mysql/
  $ xtrabackup --prepare --target-dir=/data/backup/mysql/

.. rubric:: Success Criteria

* The exit status of xtrabackup is 0.
* In the second :option:`--prepare` step, you should see InnoDB print messages
  similar to ``Log file ./ib_logfile0 did not exist: new to be created``,
  followed by a line indicating the log file was created (creating new logs is
  the purpose of the second preparation).

.. note::

   You might want to set the :option:`--use-memory` option to a value close
   to the size of your buffer pool, if you are on a dedicated server that has
   enough free memory. More details :ref:`here <xbk_option_reference>`.
   
   A more detailed explanation is :ref:`here <creating_a_backup>`.
