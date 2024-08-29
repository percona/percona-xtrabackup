.. _pxb.recipe.backup.streaming:

================================================================================
Make a Streaming Backup
================================================================================

Stream mode sends the backup to ``STDOUT`` in the ``xbstream`` format instead of
copying it to the directory named by the first argument. You can pipe the output
to a local file, or, across the network, to another server.

To extract the resulting ``xbstream`` file, you **must** use the ``xbstream``
utility: ``xbstream -x <  backup.xbstream``.

.. rubric:: Examples of Using ``xbstream``

.. include:: ../.res/contents/example.xbstream.txt

.. seealso::

   More information about streaming and compressing backups
      Section :ref:`pxb.xtrabackup.streaming`
