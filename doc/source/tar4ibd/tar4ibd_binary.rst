======================
 The |tar4ibd| Binary
======================

The |tar4ibd| binary is a specially patched version of |tar| that understands how to handle |InnoDB| / |XtraDB| data files correctly:

* It includes the |InnoDB| page checksum calculations, so it can verify that the pages it copies are not corrupt from |InnoDB| 's point of view.

* It looks at the data it copies in multiples of 4kb, from 16kB down to 4kB, to see if they are |InnoDB| pages. In this way it detects any |InnoDB| data page in the 4-16kB range automatically.

* If |InnoDB| 's checksum algorithm reports that a page is corrupt, it tries to read it again; it retries 9 times.

* If possible, it uses the following two POSIX ``fadvise()`` calls to optimize operating system cache usage, in the same fashion as |xtrabackup|: ::

   posix_fadvise(filefd, 0, 0, POSIX_FADV_SEQUENTIAL);
   posix_fadvise(filefd, 0, 0, POSIX_FADV_DONTNEED);

Special Considerations
======================

  * The |tar4ibd| binary produces archives that can be extracted with standard GNU |tar|, but you must use the ``-i`` option, or only part of your data will be extracted.

  * It is used only for creating archives with backup files from |XtraBackup|, options ``-g`` (``--listed-incremental``) and ``-z`` (``--gzip``, ``--gunzip``) are not supported and should not be used with |tar4ibd|.

    *  Compression at the moment of the backup should be achieved by streaming the backup (``--compress``) and piping through a compress utility (examples - with |innobackupex| - can be found :doc:`here <../innobackupex/streaming_backups_innobackupex>`)

    * Data from incremental backups is appended to the base backup at the preparation step, see the rationale of the process :doc:`here <../innobackupex/partial_backups_innobackupex>`.
