===================================
 Streaming and Compressing Backups
===================================

Streaming mode, supported by |Percona XtraBackup|, sends backup to ``STDOUT`` in
special ``tar`` or |xbstream| format instead of copying files to the backup
directory.

This allows you to use other programs to filter the output of the backup,
providing greater flexibility for storage of the backup. For example,
compression is achieved by piping the output to a compression utility. One of
the benefits of streaming backups and using Unix pipes is that the backups can
be automatically encrypted.

To use the streaming feature, you must use the :option:`innobackupex --stream`,
providing the format of the stream (``tar`` or ``xbstream`` ) and where to store
the temporary files::

 $ innobackupex --stream=tar /tmp

|innobackupex| uses |xbstream| to stream all of the data files to ``STDOUT``, in
a special ``xbstream`` format. See :doc:`../xbstream/xbstream` for
details. After it finishes streaming all of the data files to ``STDOUT``, it
stops xtrabackup and streams the saved log file too.

When compression is enabled, |xtrabackup| compresses all output data, except the
meta and non-InnoDB files which are not compressed, using the specified
compression algorithm. The only currently supported algorithm is
``quicklz``. The resulting files have the qpress archive format, i.e. every
\*.qp file produced by xtrabackup is essentially a one-file qpress archive and
can be extracted and uncompressed by the `qpress file archiver
<http://www.quicklz.com/>`_ which is available from :ref:`Percona Software
repositories <installation>`.

Using |xbstream| as a stream option, backups can be copied and compressed in
parallel which can significantly speed up the backup process. In case backups
were both compressed and encrypted, they'll need to decrypted first in order to
be uncompressed.

Examples using xbstream
=======================

Store the complete backup directly to a single file: ::

 $ innobackupex --stream=xbstream /root/backup/ > /root/backup/backup.xbstream

To stream and compress the backup: ::  

 $ innobackupex --stream=xbstream --compress /root/backup/ > /root/backup/backup.xbstream

To unpack the backup to the /root/backup/ directory: ::  

 $ xbstream -x <  backup.xbstream -C /root/backup/

To send the compressed backup to another host and unpack it:

.. code-block:: bash

   $ innobackupex --compress --stream=xbstream /root/backup/ | ssh user@otherhost "xbstream -x -C /root/backup/" 

Examples using tar
==================

Store the complete backup directly to a tar archive:

.. code-block:: bash

   $ innobackupex --stream=tar /root/backup/ > /root/backup/out.tar

To send the tar archive to another host:

.. code-block:: bash

   $ innobackupex --stream=tar ./ | ssh user@destination \ "cat - > /data/backups/backup.tar"

.. warning::

   To extract |Percona XtraBackup|'s archive you **must** use |tar| with ``-i`` option::

   .. code-block:: bash

      $ tar -xizf backup.tar.gz

Compress with your preferred compression tool:

.. code-block:: bash

   $ innobackupex --stream=tar ./ | gzip - > backup.tar.gz
   $ innobackupex --stream=tar ./ | bzip2 - > backup.tar.bz2

Note that the streamed backup will need to be prepared before
restoration. Streaming mode does not prepare the backup.

