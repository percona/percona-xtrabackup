.. _pxb.xtrabackup.streaming:

================================================================================
Streaming Backups
================================================================================

Streaming mode, supported by |Percona XtraBackup|, sends backup to ``STDOUT`` in
the |xbstream| format instead of copying files to the backup directory.

This allows you to use other programs to filter the output of the backup,
providing greater flexibility for storage of the backup. For example,
compression is achieved by piping the output to a compression utility. One of
the benefits of streaming backups and using Unix pipes is that the backups can
be automatically encrypted.

To use the streaming feature, you must use the :option:`--stream`,
providing the format of the stream (``xbstream`` ) and where to store
the temporary files::

 $ xtrabackup --stream=xbstream --target-dir=/tmp

*Percona XtraBackup* uses |xbstream| to stream all of the data files to ``STDOUT``, in a
special ``xbstream`` format. After it finishes streaming all of the data files
to ``STDOUT``, it stops xtrabackup and streams the saved log file too.

.. seealso::

   More information about |xbstream|
      :ref:`xbstream_binary`

When compression is enabled, *Percona XtraBackup* compresses all output data, except the
meta and non-InnoDB files which are not compressed, using the specified
compression algorithm. The only currently supported algorithm is
``quicklz``. The resulting files have the ``qpress`` archive format, i.e. every
\*.qp file produced by xtrabackup is essentially a one-file qpress archive and
can be extracted and uncompressed by the `qpress file archiver
<http://www.quicklz.com/>`_ which is available from :ref:`Percona Software
repositories <installation>`.

Using |xbstream| as a stream option, backups can be copied and compressed in
parallel which can significantly speed up the backup process. In case backups
were both compressed and encrypted, they'll need to decrypted first in order to
be uncompressed.

.. list-table::
   :widths: 25 75
   :header-rows: 1
		 
   * - Task
     - Command
   * - Stream the backup into an archive named :file:`backup.xbstream`
     - :bash:`xtrabackup --backup --stream=xbstream --target-dir=./ > backup.xbstream`
   * - Stream the backup into a `compressed` archive named `backup.xbstream`
     - :bash:`xtrabackup --backup --stream=xbstream --compress --target-dir=./ > backup.xbstream`
   * - Encrypt the backup
     - $ xtrabackup --backup \
       --stream=xbstream ./ > backup.xbstream \
       gzip -  | openssl des3 -salt -k "password" > backup.xbstream.gz.des3

   * - Unpack the backup to the current directory
     - :bash:`xbstream -x <  backup.xbstream`
   * - Send the compressed backup directly to another host and unpack it
     - :bash:`xtrabackup --backup --compress --stream=xbstream --target-dir=./ | ssh user@otherhost "xbstream -x"`
   * - Send the backup to another server using ``netcat``.
     - On the destination host:
 
       .. code-block:: bash
 
          $ nc -l 9999 | cat - > /data/backups/backup.xbstream
 
       On the source host:
      
       .. code-block:: bash
 
          $ xtrabackup --backup --stream=xbstream ./ | nc desthost 9999
 
   * - Send the backup to another server using a one-line command:
     - $ ssh user@desthost "( nc -l 9999 > /data/backups/backup.xbstream & )" \
       && xtrabackup --backup --stream=xbstream ./ |  nc desthost 9999
          
   * - Throttle the throughput to 10MB/sec using the ``pipe viewer`` tool. Install from the `official site <http://www.ivarch.com/programs/quickref/pv.shtml>`_ or from the distribution package (``apt install pv``)
     - $ xtrabackup --backup --stream=xbstream ./ | pv -q -L10m \
       ssh user@desthost "cat - > /data/backups/backup.xbstream"
 
   * - Checksum the backup during streaming:
     - On the destination host:
 
       .. code-block:: bash
 
          $ nc -l 9999 | tee >(sha1sum > destination_checksum) > /data/backups/backup.xbstream
 
       On the source host:
      
       .. code-block:: bash
 
          $ xtrabackup --backup --stream=xbstream ./ | tee >(sha1sum > source_checksum) | nc desthost 9999
 
       Compare the checksums on the source host:
 
       .. code-block:: bash
 
          $ cat source_checksum 
          65e4f916a49c1f216e0887ce54cf59bf3934dbad  -
 
       Compare the checksums on the destination host:
 
       .. code-block:: bash
 
          $ cat destination_checksum 
          65e4f916a49c1f216e0887ce54cf59bf3934dbad  -

   * - Parallel compression with parallel copying backup
     - :bash:`xtrabackup --backup --compress --compress-threads=8 --stream=xbstream --parallel=4 --target-dir=./ > backup.xbstream`


.. note:: The streamed backup must be prepared before restoration. The Streaming mode does not prepare the backup.

