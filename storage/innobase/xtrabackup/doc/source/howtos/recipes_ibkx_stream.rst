=========================
 Make a Streaming Backup
=========================

Stream mode sends the backup to ``STDOUT`` in the xbstream format instead of copying it to the directory named by the first argument. You can pipe the output to :command:`gzip`, or across the network, to another server.

To extract the resulting xbstream file, you **must** use the |xbstream| utility: ``xbstream -x <  backup.xbstream``.

Here are some examples of using the ``xbstream`` option for streaming:

.. list-table::

   * - Action
     - Command
   * - Stream the backup into a xbstream archived named 'backup.xbstream'
     - $ xtrabackup --backup --stream=xbstream ./ > backup.xbstream
   * - Stream the backup into a xbstream archived named 'backup.xbstream' and compress it
     - $ xtrabackup --backup --stream=xbstream --compress ./ > backup.xbstream
   * - Encrypt the backup
     - $ xtrabackup --backup \
       --stream=xbstream ./ > backup.xbstream \
       gzip -  | openssl des3 -salt -k "password" > backup.xbstream.gz.des3
   * - Send the backup to another server and unpack it
     - $ xtrabackup --backup --compress --stream=xbstream ./ | ssh user@otherhost "xbstream -x"
   * - Send the backup to another server using ``netcat``.
     - On the destination host:

       .. code-block:: bash

	  $ nc -l 9999 | cat - > /data/backups/backup.xbstream

       On the source host:
      
       .. code-block:: bash

	  $ xtrabackup --backup --stream=xbstream ./ | nc desthost 9999

   * - Send the backup to another server using a one-liner:
     - $ ssh user@desthost "( nc -l 9999 > /data/backups/backup.xbstream & )" \
       && xtrabackup --backup --stream=xbstream ./ |  nc desthost 9999
          
   * - Throttle the throughput to 10MB/sec using the ``pipe viewer`` tool [#f1]_
     - $ xtrabackup --backup --stream=xbstream ./ | pv -q -L10m \
       ssh user@desthost "cat - > /data/backups/backup.xbstream"

   * - Checksumming the backup during the streaming:
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
 
   * - Parallel compression with parallel copying backup:
     - $ xtrabackup --backup --compress --compress-threads=8 \
       --stream=xbstream --parallel=4 ./ > backup.xbstream

.. rubric:: Footnotes

.. [#f1] Install from the `official site <http://www.ivarch.com/programs/quickref/pv.shtml>`_ or from the distribution package (``apt install pv``)
