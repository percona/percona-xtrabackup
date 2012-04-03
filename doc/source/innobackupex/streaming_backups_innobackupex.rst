===================================
 Streaming and Compressing Backups
===================================

Streaming mode, supported by |XtraBackup|, sends backup to ``STDOUT`` in special ``tar`` format instead of copying files to the backup directory.

This allows to pipe the stream to other programs, providing great flexibility to the output of it. For example, compression is achieved by piping the output to a compression utility.

To use this feature, you must use the :option:`--stream`, providing the format of the stream (only ``tar`` is supported at this moment) and where should the store the temporary files::

 $ innobackupex --stream=tar /tmp

|innobackupex| starts |xtrabackup| in :option:`--log-stream` mode in a child process, and redirects its log to a temporary file. It then uses |tar4ibd| to stream all of the data files to ``STDOUT``, in a special ``tar`` format. See :doc:`../tar4ibd/tar4ibd_binary` for details. After it finishes streaming all of the data files to ``STDOUT``, it stops xtrabackup and streams the saved log file too.

To store the backup in one archive it directly :: 

 $ innobackupex --stream=tar /root/backup/ > /root/backup/out.tar

For sending it directly to another host by ::

 $ innobackupex --stream=tar ./ | ssh user@destination \ "cat - > /data/backups/backup.tar"

.. warning::  To extract |XtraBackup| 's archive you **must** use |tar| with ``-i`` option::

  $ tar -xizf backup.tar.gz

Choosing the compression tool that best suits you: :: 

 $ innobackupex --stream=tar ./ | gzip - > backup.tar.gz
 $ innobackupex --stream=tar ./ | bzip2 - > backup.tar.bz2

Note that the streamed backup will need to be prepared before restoration. Streaming mode does not prepare the backup.
