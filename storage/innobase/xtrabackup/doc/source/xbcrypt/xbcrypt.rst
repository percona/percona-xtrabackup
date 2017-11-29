.. _xbcrypt:

==================
The xbcrypt binary
==================

To support encryption and decryption of the backups, a new tool ``xbcrypt`` was
introduced to |Percona XtraBackup|.

This utility has been modeled after :ref:`xbstream_binary` to perform
encryption and decryption outside of |Percona XtraBackup|. ``xbcrypt`` has
following command line options:

.. option:: -d, --decrypt

   Decrypt data input to output.

.. option::  -i, --input=name

   Optional input file. If not specified, input will be read from standard
   input.

.. option::  -o, --output=name

   Optional output file. If not specified, output will be written to standard
   output.

.. option:: -a, --encrypt-algo=name

   Encryption algorithm.

.. option:: -k, --encrypt-key=name

   Encryption key.

.. option:: -f, --encrypt-key-file=name

   File which contains encryption key.

.. option:: -s, --encrypt-chunk-size=#

   Size of working buffer for encryption in bytes. The default value is 64K.

.. option:: --encrypt-threads=#

   This option specifies the number of worker threads that will be used for
   parallel encryption/decryption.

.. option:: -v, --verbose

   Display verbose status output.
