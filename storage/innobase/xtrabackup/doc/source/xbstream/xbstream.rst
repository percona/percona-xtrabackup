.. _xbstream_binary:

===================
The xbstream binary
===================

To support simultaneous compression and streaming, a new custom streaming
format called xbstream was introduced to *Percona XtraBackup* in addition to
the TAR format. That was required to overcome some limitations of traditional
archive formats such as tar, cpio and others which did not allow streaming
dynamically generated files, for example dynamically compressed files. Other
advantages of xbstream over traditional streaming/archive format include
ability to stream multiple files concurrently (so it is possible to use
streaming in the xbstream format together with the --parallel option) and more
compact data storage.

This utility has a tar-like interface:

 - with the ``-x`` option it extracts files from the stream read from its
   standard input to the current directory unless specified otherwise with the
   ``-c`` option. Support for parallel extraction with the ``--parallel``
   option has been implemented in *Percona XtraBackup* 2.4.7.

 - with the ``-c`` option it streams files specified on the command line to its
   standard output.

 - with the ``--decrypt=ALGO`` option specified xbstream will automatically
   decrypt encrypted files when extracting input stream. Supported values for
   this option are: ``AES128``, ``AES192``, and ``AES256``. Either
   ``--encrypt-key`` or ``--encrypt-key-file`` options must be specified to
   provide encryption key, but not both. This option has been implemented in
   *Percona XtraBackup* 2.4.7.

 - with the ``--encrypt-threads`` option you can specify the number of threads
   for parallel data encryption. The default value is ``1``. This option has
   been implemented in *Percona XtraBackup* 2.4.7.

 - the ``--encrypt-key`` option is used to specify the encryption key that will
   be used. It can't be used with ``--encrypt-key-file`` option because they
   are mutually exclusive. This option has been implemented in *Percona
   XtraBackup* 2.4.7.

 - the ``--encrypt-key-file`` option is used to specify the file that contains
   the encryption key. It can't be used with ``--encrypt-key`` option.
   because they are mutually exclusive. This option has been implemented in
   *Percona XtraBackup* 2.4.7.

The utility also tries to minimize its impact on the OS page cache by using the
appropriate ``posix_fadvise()`` calls when available.

When compression is enabled with *xtrabackup* all data is being compressed,
including the transaction log file and meta data files, using the specified
compression algorithm. The only currently supported algorithm is ``quicklz``.

The resulting files have the qpress archive format, i.e., every ``*.qp`` file
produced by *xtrabackup* is essentially a one-file qpress archive and can be
extracted and uncompressed by the `qpress file archiver
<http://www.quicklz.com/>`_. This means that there is no need to decompress
entire backup to restore a single table as with :file:`tar.gz`.

To decompress individual files, run *xbstream* with the
:option:`--decompress` option. You may control the number of threads
used for decompressing by passing the :option:`--decompress-threads`
option.

Also, files can be decompressed using the **qpress** tool that can be downloaded from
`here <http://www.quicklz.com/>`_. Qpress supports multi-threaded decompression.
