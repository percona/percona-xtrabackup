.. _xtrabackup.full:

================================================================================
Make a Full Backup
================================================================================

Assumptions
--------------------

Most of the times, the context will make the recipe or tutorial understandable.
To assure that, a list of the assumptions, names and "things" that will appear
in this section is given. At the beginning of each recipe or tutorial they will
be specified in order to make it quicker and more practical.

``HOST``

A system with a |MySQL|-based server installed, configured, and running. We assume the following about this system:

* Can :ref:`enable-tcpip`

* a SSH server is installed and configured - see :doc:`here <howtos/ssh_server>` if it is not;

* you have an user account in the system with the appropriate :doc:`permissions <howtos/permissions>` and

* you have a MySQL's user account with appropriate :ref:`privileges`.

``USER``
   A system account with shell access and the appropriate permissions
   for the task. A guide for checking them is :doc:`here <howtos/permissions>`.

``DB-USER``
   A database server account with the appropriate privileges for the
   task. A guide for checking them is :doc:`here <howtos/permissions>`.


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
