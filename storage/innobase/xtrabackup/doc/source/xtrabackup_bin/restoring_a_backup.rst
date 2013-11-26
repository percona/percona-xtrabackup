Restoring a Backup
==================

The |xtrabackup| binary does not have any functionality for restoring a backup. That is up to the user to do. You might use :program:`rsync` or :program:`cp` to restore the files. You should check that the restored files have the correct ownership and permissions.

.. note:: 

 The :term:`datadir` must be empty before restoring the backup. Also it's important to note that MySQL server needs to be shut down before restore is performed. You can't restore to a :term:`datadir` of a running mysqld instance (except when importing a partial backup). 

Example of the :program:`rsync` command that can be used to restore the backup can look like this: ::
 
 $ rsync -avrP /data/backup/ /var/lib/mysql/

As files' attributes will be preserved, in most cases you will need to change the files' ownership to ``mysql`` before starting the database server, as they will be owned by the user who created the backup::

  $ chown -R mysql:mysql /var/lib/mysql

Note that |xtrabackup| backs up only the |InnoDB| data. You must separately restore the |MySQL| system database, |MyISAM| data, table definition files (:term:`.frm` files), and everything else necessary to make your database functional -- or |innobackupex| :doc:`can do it for you <../innobackupex/restoring_a_backup_ibk>`.
