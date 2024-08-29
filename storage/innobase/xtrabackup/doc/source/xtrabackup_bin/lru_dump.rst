================================================================================
LRU dump backup
================================================================================

.. There is a MySQL feature
   https://dev.mysql.com/doc/refman/8.0/en/innodb-preload-buffer-pool.html, it
   allows to save and restore buffer pool dump. xtrabackup includes saved buffer
   pool dump into a backup

   ib_lru_dump should be renamed to ib_buffer_pool and reference is given on mysql docs
   on how to enable it


*Percona XtraBackup* includes a saved buffer pool dump into a backup to enable
reducing the warm up time. It restores the buffer pool state from
:file:`ib_buffer_pool` file after restart. *Percona XtraBackup* discovers
:file:`ib_buffer_pool` and backs it up automatically.

.. image:: /_static/lru_dump.png

If the buffer restore option is enabled in :file:`my.cnf` buffer pool will be in
the warm state after backup is restored.

.. seealso::

   *MySQL* Documentation: Saving and Restoring the Buffer Pool State
      https://dev.mysql.com/doc/refman/8.0/en/innodb-preload-buffer-pool.html
