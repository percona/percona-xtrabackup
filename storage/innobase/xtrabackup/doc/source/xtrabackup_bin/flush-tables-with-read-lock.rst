.. _pxb.xtrabackup.flush-tables-with-read-lock:

================================================================================
``FLUSH TABLES WITH READ LOCK`` option
================================================================================

The ``FLUSH TABLES WITH READ LOCK`` option does the following with a global read lock:

* Closes all open tables

* Locks all tables for all databases

Release the lock with ``UNLOCK TABLES``.

.. note::

   ``FLUSH TABLES WITH READ LOCK`` does not prevent inserting rows into the log tables.
   

To ensure consistent backups, use the ``FLUSH TABLES WITH READ LOCK`` option before taking a non-InnoDB file backup. The option does not affect long-running queries.

Long-running queries with ``FLUSH TABLES WITH READ LOCK`` enabled can leave the server in a read-only mode until the queries finish. Killing the ``FLUSH TABLES WITH READ LOCK`` does not help if the database is in either the ``Waiting for table flush`` or ``Waiting for master to send event`` state. To return to normal operation, you must kill any long-running queries.

.. note:: 

   All described in this section has no effect when backup locks are
   used. *Percona XtraBackup* will use `Backup locks
   <https://www.percona.com/doc/percona-server/5.6/management/backup_locks.html#backup-locks>`_
   where available as a lightweight alternative to ``FLUSH TABLES WITH READ
   LOCK``. This feature is available in *Percona Server for MySQL* 5.6+. 
   *Percona XtraBackup* uses this automatically to copy non-InnoDB data to avoid blocking
   DML queries that modify InnoDB tables.

In order to prevent this from happening two things have been implemented:

* *xtrabackup* waits for a good moment to issue the global lock
* *xtrabackup* kills all queries or only the SELECT queries which prevent the
  global lock from being acquired

Waiting for queries to finish
================================================================================

You should issue a global lock when no long queries are running. Waiting to issue the global lock for extended period of time is not a good method. The wait can extend the time needed for
backup to take place. The `--ftwrl-wait-timeout` option can limit the
waiting time. If it cannot issue the lock during this
time, *xtrabackup* stops the option, exits with an error message, and backup is
not be taken.

The default value for this option is zero (0) value which turns off the option.

Another possibility is to specify the type of query to wait on. In this case
:option:`--ftwrl-wait-query-type`. Possible values are ``all`` and
``update``. When ``all`` is used *xtrabackup* will wait for all long running
queries (execution time longer than allowed by :option:`--ftwrl-wait-threshold`)
to finish before running the ``FLUSH TABLES WITH READ LOCK``. When ``update`` is
used *xtrabackup* will wait on ``UPDATE/ALTER/REPLACE/INSERT`` queries to
finish.

The time needed for a specific query to complete is hard to predict. We assume that the long-running queries will not finish in a timely manner. Other queries which run for a short time finish quickly. *xtrabackup* uses the value of
`--ftwrl-wait-threshold` option to specify the long-running queries
and will block a global lock. In order to use this option
xtrabackup user should have ``PROCESS`` and ``SUPER`` privileges.

Killing the blocking queries
================================================================================

The second option is to kill all the queries which prevent from acquiring the
global lock. In this case, all queries which run longer than ``FLUSH TABLES WITH
READ LOCK`` are potential blockers. Although all queries can be killed,
additional time can be specified for the short running queries to finish using
the :option:`--kill-long-queries-timeout` option. This option
specifies the time for queries to complete, after the value is reached, all the
running queries will be killed. The default value is zero, which turns this
feature off.

The :option:`--kill-long-query-type` option can be used to specify all or only
``SELECT`` queries that are preventing global lock from being acquired. In order
to use this option xtrabackup user should have ``PROCESS`` and ``SUPER``
privileges.

Options summary
================================================================================

* :option:`--ftwrl-wait-timeout` (seconds) - how long to wait for a
  good moment. Default is 0, not to wait.
* :option:`--ftwrl-wait-query-type` - which long queries
  should be finished before ``FLUSH TABLES WITH READ LOCK`` is run. Default is
  all.
* :option:`--ftwrl-wait-threshold` (seconds) - how long query
  should be running before we consider it long running and potential blocker of
  global lock.
* :option:`--kill-long-queries-timeout` (seconds) - how many time
  we give for queries to complete after ``FLUSH TABLES WITH READ LOCK`` is
  issued before start to kill. Default if ``0``, not to kill.
* :option:`--kill-long-query-type` - which queries should be killed once
  ``kill-long-queries-timeout`` has expired.

Example
--------------------------------------------------------------------------------

Running the *xtrabackup* with the following options will cause *xtrabackup*
to spend no longer than 3 minutes waiting for all queries older than 40 seconds
to complete.

.. code-block:: bash

   $  xtrabackup --backup --ftwrl-wait-threshold=40 \
   --ftwrl-wait-query-type=all --ftwrl-wait-timeout=180 \
   --kill-long-queries-timeout=20 --kill-long-query-type=all \
   --target-dir=/data/backups/


After ``FLUSH TABLES WITH READ LOCK`` is issued, *xtrabackup* will wait for 20
seconds for lock to be acquired. If lock is still not acquired after 20 seconds,
it will kill all queries which are running longer that the ``FLUSH TABLES WITH
READ LOCK``.

