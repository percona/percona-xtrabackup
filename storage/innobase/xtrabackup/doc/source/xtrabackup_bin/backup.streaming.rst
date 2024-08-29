.. _pxb.xtrabackup.streaming:

================================================================================
Streaming Backups
================================================================================

**Percona XtraBackup** supports streaming mode. Streaming mode sends a backup to ``STDOUT`` in the *xbstream* format instead of copying the files to the backup directory.

This method allows you to use other programs to filter the output of the backup,
providing greater flexibility for storage of the backup. For example,
compression is achieved by piping the output to a compression utility. One of
the benefits of streaming backups and using Unix pipes is that the backups can
be automatically encrypted.

To use the streaming feature, you must use the :option:`--stream`,
providing the format of the stream (``xbstream`` ) and where to store
the temporary files::

 $ xtrabackup --stream=xbstream --target-dir=/tmp

*xtrabackup* uses *xbstream* to stream all of the data files to ``STDOUT``, in a
special ``xbstream`` format. After it finishes streaming all of the data files
to ``STDOUT``, it stops xtrabackup and streams the saved log file too.

.. seealso::

   More information about *xbstream*
      :ref:`xbstream_binary`

When compression is enabled, *xtrabackup* compresses the output data, except for the meta and non-InnoDB files which are not compressed, using the specified
compression algorithm. The only currently supported algorithm is
``quicklz``. The resulting files have the ``qpress`` archive format, i.e. every
\*.qp file produced by xtrabackup is essentially a one-file qpress archive and
can be extracted and uncompressed by the `qpress file archiver
<http://www.quicklz.com/>`_ which is available from :ref:`Percona Software
repositories <installation>`.

Using *xbstream* as a stream option, backups can be copied and compressed in
parallel. This option can significantly improve the speed of the backup process. In case backups
were both compressed and encrypted, they must be decrypted before they are uncompressed.

.. include:: ../.res/contents/example.xbstream.txt

Note that the streamed backup will need to be prepared before
restoration. Streaming mode does not prepare the backup.

