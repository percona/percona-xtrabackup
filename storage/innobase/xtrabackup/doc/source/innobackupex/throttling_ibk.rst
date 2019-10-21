========================================
 Throttling backups with |innobackupex|
========================================

Although |innobackupex| does not block your database's operation, any backup can
add load to the system being backed up. On systems that do not have much spare
I/O capacity, it might be helpful to throttle the rate at which |innobackupex|
reads and writes |InnoDB| data. You can do this with the
:option:`innobackupex --throttle` option.

This option is passed directly to |xtrabackup| binary and only limits the
operations on the logs and files of |InnoDB| tables. It doesn't have an effect
on reading or writing files from tables with other storage engine.

One way of checking the current I/O operations at a system is with
:command:`iostat` command. See :ref:`throttling_backups` for details of how
throttling works.

.. note:: 

   :option:`innobackupex --throttle` option works only during the backup phase,
   i.e. it will not work with :option:`innobackupex --apply-log` and
   :option:`innobackupex --copy-back` options.

The :option:`innobackupex --throttle` option is similar to the ``--sleep``
option in ``mysqlbackup`` and should be used instead of it, as ``--sleep`` will
be ignored.
