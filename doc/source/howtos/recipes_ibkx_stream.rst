=========================
 Make a Streaming Backup
=========================

Stream mode sends the backup to ``STDOUT`` in tar format instead of copying it to the directory named by the first argument. You can pipe the output to :command:`gzip`, or across the network to another server.

To extract the resulting tar file, you **must** use the ``-i`` option, such as ``tar -ixvf backup.tar``.

.. warning:: Remember to use the ``-i`` option for extracting a tarred backup. For more information, see :doc:`../innobackupex/streaming_backups_innobackupex`.

Here are some examples:

.. code-block:: console

  ## Stream the backup into a tar archived named 'backup.tar'
  innobackupex --stream=tar ./ > backup.tar

  ## The same, but compress it
  innobackupex --stream=tar ./ | gzip - > backup.tar.gz

  ## Send it to another server instead of storing it locally
  innobackupex --stream=tar ./ | ssh user@desthost "cat - > /data/backups/backup.tar"

  ## The same thing with ''netcat'' (faster).  On the destination host:
  nc -l 9999 | cat - > /data/backups/backup.tar
  ## On the source host:
  innobackupex --stream=tar ./ | nc desthost 9999

  ## The same thing, but done as a one-liner:
  ssh user@desthost "( nc -l 9999 > /data/backups/backup.tar & )" \
  && innobackupex --stream=tar ./  |  nc desthost 9999

  ## Throttle the throughput to 10MB/sec.  Requires the 'pv' tools; you
  ## can find them at http://www.ivarch.com/programs/quickref/pv.shtml
  innobackupex --stream=tar ./ | pv -q -L10m \
  | ssh user@desthost "cat - > /data/backups/backup.tar"
