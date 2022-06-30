.. _xbcrypt:

==================
The xbcrypt binary
==================

To support encryption and decryption of the backups, a new tool ``xbcrypt`` was
introduced to *Percona XtraBackup*.

**Percona XtraBackup** 8.0.28-20 implements the XBCRYPT_ENCRYPTION_KEY environment variable. The variable is only used in place of the ``--encrypt_key=name`` option. You can use the environment variable or command line option. If you use both, the command line option takes precedence over the value specified in the environment variable.

This utility has been modeled after :ref:`xbstream_binary` to perform
encryption and decryption outside of *Percona XtraBackup*. ``xbcrypt`` has
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
