.. _pxb.xtrabackup.flush-tables-with-read-lock:

================================================================================
Handling ``FLUSH TABLES WITH READ LOCK``
================================================================================

When making backups, ``FLUSH TABLES WITH READ LOCK`` is used before the
non-InnoDB files are backed up to ensure that the backup is consistent. ``FLUSH
TABLES WITH READ LOCK`` can be run even though there may be a running query that
has been executing for hours.

In this case, everything is locked in the ``Waiting for table flush`` or
``Waiting for master to send event`` state. Killing the ``FLUSH TABLES WITH
READ LOCK`` does not correct this problem. The only way to get
the server operating normally again is to kill off the long running queries that
blocked it to begin with. This means that if there are long running queries
``FLUSH TABLES WITH READ LOCK`` can get stuck, leaving server in read-only mode
until waiting for these queries to complete.

.. note:: 

   All described in this section has no effect when backup locks are
   used. |Percona XtraBackup| will use `Backup locks
   <https://www.percona.com/doc/percona-server/5.6/management/backup_locks.html#backup-locks>`_
   where available as a lightweight alternative to ``FLUSH TABLES WITH READ
   LOCK``. This feature is available in |Percona Server| 5.6+. |Percona
   XtraBackup| uses this automatically to copy non-InnoDB data to avoid blocking
   DML queries that modify InnoDB tables.

In order to prevent this from happening two things have been implemented:

* *Percona XtraBackup* can wait for a good moment to issue the global lock.
* *Percona XtraBackup* can kill all or only SELECT queries which are preventing the
  global lock from being acquired

Waiting for queries to finish
================================================================================

Good moment to issue a global lock is the moment when there are no long queries
running. But waiting for a good moment to issue the global lock for extended
period of time isn't always good approach, as it can extend the time needed for
backup to take place. To prevent *Percona XtraBackup* from waiting to issue ``FLUSH
TABLES WITH READ LOCK`` for too long, new option has been implemented:
:option:`--ftwrl-wait-timeout` option can be used to limit the
waiting time. If the good moment to issue the lock did not happen during this
time, *Percona XtraBackup* will give up and exit with an error message and backup will
not be taken. Zero value for this option turns off the feature (which is
default).

Another possibility is to specify the type of query to wait on. In this case
:option:`--ftwrl-wait-query-type`. Possible values are ``all`` and
``update``. When ``all`` is used *Percona XtraBackup* will wait for all long running
queries (execution time longer than allowed by :option:`--ftwrl-wait-threshold`)
to finish before running the ``FLUSH TABLES WITH READ LOCK``. When ``update`` is
used *Percona XtraBackup* will wait on ``UPDATE/ALTER/REPLACE/INSERT`` queries to
finish.

Although the time needed for a specific query to complete is hard to predict, we
can assume that the queries that have been running for a long time are not
likely to finish soon. The queries which are running for a short time are likely
to finish shortly. *Percona XtraBackup* can use the value of
:option:`--ftwrl-wait-threshold` option to specify which query is long running
and will likely block global lock for a while. In order to use this option
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

Running the *Percona XtraBackup* with the following options will cause *Percona XtraBackup*
to spend no longer than 3 minutes waiting for all queries older than 40 seconds
to complete.

.. code-block:: bash

   $  xtrabackup --backup --ftwrl-wait-threshold=40 \
   --ftwrl-wait-query-type=all --ftwrl-wait-timeout=180 \
   --kill-long-queries-timeout=20 --kill-long-query-type=all \
   --target-dir=/data/backups/


After ``FLUSH TABLES WITH READ LOCK`` is issued, *Percona XtraBackup* will wait for 20
seconds for lock to be acquired. If lock is still not acquired after 20 seconds,
it will kill all queries which are running longer that the ``FLUSH TABLES WITH
READ LOCK``.

