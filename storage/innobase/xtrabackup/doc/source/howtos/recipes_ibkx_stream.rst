=========================
 Make a Streaming Backup
=========================

Stream mode sends the backup to ``STDOUT`` in the ``xbstream`` format instead of copying it to the directory named by the first argument. You can pipe the output to a local file, or, across the network, to another server.

To extract the resulting ``xbstream`` file, you **must** use the ``xbstream`` utility.

.. rubric:: Examples of Using ``xbstream``

.. list-table::
   :header-rows: 1
		 
   * - Task
     - Command
   * - Stream the backup into an archive named :file:`backup.xbstream`
     - :code:`$ xtrabackup --backup --stream=xbstream ./ > backup.tar`
   * - Stream the backup into a `compressed` archive named :file:`backup.xbstream`
     - :code:`$ innobackupex --backup --stream=xbstream --compress ./ | gzip - > backup.tar.gz`
   * - Unpack the backup to the current directory
     - :code:`$ xbstream -x <  backup.xbstream`
   * - Send the backup compressed directly to another host and unpack it
     - :code:`$ xtrabackup --backup --compress --stream=xbstream ./ | ssh user@otherhost "xbstream -x"`
   * - Parallel compression with parallel copying backup
     - :code:`$ xtrabackup --backup --compress --compress-threads=8 --stream=xbstream --parallel=4 ./ > backup.xbstream`
