.. _throttling_backups_xbk:

Throttling Backups
==================

Although xtrabackup does not block your database's operation, any backup can add
load to the system being backed up. On systems that do not have much spare I/O
capacity, it might be helpful to throttle the rate at which xtrabackup reads and
writes data. You can do this with the :option:`--throttle` option.

Image below shows how throttling works when :option:`--throttle` =1. 

.. image:: /_static/throttle.png

By default, there is no throttling, and xtrabackup reads and writes data as
quickly as it can. If you set too strict of a limit on the I/O operations, the
backup might be so slow that it will never catch up with the transaction logs
that InnoDB is writing, so the backup might never complete.

