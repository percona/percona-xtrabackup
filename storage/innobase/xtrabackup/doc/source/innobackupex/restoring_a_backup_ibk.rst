=============================================
 Restoring a Full Backup with |innobackupex|
=============================================

For convenience, |innobackupex| has a :option:`innobackupex --copy-back` option,
which performs the restoration of a backup to the server's :term:`datadir`:

.. code-block:: bash

   $ innobackupex --copy-back /path/to/BACKUP-DIR

It will copy all the data-related files back to the server's :term:`datadir`,
determined by the server's :file:`my.cnf` configuration file. You should check
the last line of the output for a success message::

  innobackupex: Finished copying back files.

  111225 01:08:13  innobackupex: completed OK!

.. note:: 

   The :term:`datadir` must be empty; |Percona XtraBackup| :option:`innobackupex --copy-back`
   option will not copy over existing files unless
   :option:`innobackupex --force-non-empty-directories` option is
   specified. Also it is important to note that |MySQL| server needs to be shut
   down before restore is performed. You can't restore to a :term:`datadir` of a
   running mysqld instance (except when importing a partial backup).

As files' attributes will be preserved, in most cases you will need to change
the files' ownership to ``mysql`` before starting the database server, as they
will be owned by the user who created the backup:

.. code-block:: bash

   $ chown -R mysql:mysql /var/lib/mysql

Also note that all of these operations will be done as the user calling
|innobackupex|, you will need write permissions on the server's :term:`datadir`.
