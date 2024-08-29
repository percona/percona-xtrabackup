.. _recipes_ibkx_compressed:

================================================================================
Making a Compressed Backup 
================================================================================

In order to make a compressed backup, use the :option:`--compress` option along
with the :option:`--backup` and :option:`--target-dir` options. By default
:option:`--compress` uses the ``quicklz`` compression algorithm. 

.. code-block:: bash

   $ xtrabackup --backup --compress --target-dir=/data/backup

You can also use the ``lz4`` compression algorithm by setting :option:``--compress`` to ``lz4``.

.. code-block:: bash

   $ xtrabackup --backup --compress=lz4 --target-dir=/data/backup

If you want to speed up the compression you can use the parallel
compression, which can be enabled with :option:`--compress-threads`
option. The following example uses four threads for compression.

.. code-block:: bash

   $ xtrabackup --backup --compress --compress-threads=4 --target-dir=/data/backup

Output should look like this :: 

   ...
        
   [01] Compressing ./imdb/comp_cast_type.ibd to /data/backup/imdb/comp_cast_type.ibd.qp
   [01]        ...done
   ...
   130801 11:50:24  xtrabackup: completed OK

.. _recipe.xtrabackup.backup.preparing:

Preparing the backup
--------------------------------------------------------------------------------

Before you can prepare the backup you'll need to uncompress all the files with
`qpress <http://www.quicklz.com/>`_ (which is available from `Percona Software
repositories
<http://www.percona.com/doc/percona-xtrabackup/8.0/installation.html#using-percona-software-repositories>`_).
You can use the following one-liner to uncompress all the files:

.. code-block:: bash

   $ for bf in `find . -iname "*\.qp"`; do qpress -d $bf $(dirname $bf) && rm $bf; done

If you used the ``lz4`` compression algorithm change this script to search for ``*.lz4`` files:

.. code-block:: bash

   $ for bf in `find . -iname "*\.lz4"`; do lz4 -d $bf $(dirname $bf) && rm $bf; done

You can decompress the backup by using the :option:`--decompress` option:

.. code-block:: bash

   $ xtrabackup --decompress --target-dir=/data/backup/

.. seealso::

   What is required to use the `--decompress` option effectively?
      :option:`--decompress`

When the files are uncompressed you can prepare the backup with the
:option:`--apply-log-only` option:

.. code-block:: bash

   $ xtrabackup --apply-log-only --target-dir=/data/backup/

You should check for a confirmation message: ::

   130802 02:51:02  xtrabackup: completed OK!

Now the files in :file:`/data/backup/` is ready to be used by the server.

.. note::

   *Percona XtraBackup* doesn't automatically remove the compressed files. In
   order to clean up the backup directory users should remove the :file:`*.qp`
   files.

.. _recipe.xtrabackup.backup.restoring:

Restoring the backup
--------------------------------------------------------------------------------

Once the backup has been prepared you can use the :option:`--copy-back` to
restore the backup.

.. code-block:: bash

  $ xtrabackup --copy-back --target-dir=/data/backup/

This will copy the prepared data back to its original location as defined by the
``datadir`` variable in your :term:`my.cnf`.

After the confirmation message, you should check the file permissions after
copying the data back.

.. code-block:: text

   130802 02:58:44  xtrabackup: completed OK!

You may need to adjust the file permissions. The following example demonstrates
how to do it recursively by using :program:`chown`:

.. code-block:: bash

   $ chown -R mysql:mysql /var/lib/mysql

Now, your :term:`data directory <datadir>` contains the restored data. You are
ready to start the server.
