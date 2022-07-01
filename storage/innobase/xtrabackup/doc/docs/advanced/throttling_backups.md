# Throttling Backups

Although xtrabackup does not block your database’s operation, any backup can add
load to the system being backed up. On systems that do not have much spare I/O
capacity, it might be helpful to throttle the rate at which xtrabackup reads and
writes data. You can do this with the `xtrabackup --throttle`
option. This option limits the number of chunks copied per second. The chunk
size is *10 MB*.

The image below shows how throttling works when `xtrabackup --throttle` is set to 1.

![image](_static/throttle.png)

By default, there is no throttling, and xtrabackup reads and writes data as
quickly as possible. If you set too strict of a limit on the IOPS, the backup may slow down so much that it will never catch up with the transaction logs that InnoDB is writing, and the backup might never be complete.
