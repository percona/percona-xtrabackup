.. _encrypted_backups_ibk:

===================
 Encrypted Backups
===================

|Percona XtraBackup| has implemented support for encrypted backups. This feature was introduced in |Percona XtraBackup| 2.1. It can be used to encrypt/decrypt local or streaming backup with |xbstream| option (streaming tar backups are not supported) in order to add another layer of protection to the backups. Encryption is done with the ``libgcrypt`` library.

Creating Encrypted Backups
===========================

To make an encrypted backup following options need to be specified (options :option:`--encrypt-key` and :option:`--encrypt-key-file` are mutually exclusive, i.e. just one of them needs to be provided): 

 * :option:`--encryption=ALGORITHM` - currently supported algorithms are: ``AES128``, ``AES192`` and ``AES256``

 * :option:`--encrypt-key=ENCRYPTION_KEY` - proper length encryption key to use. It is not recommended to use this option where there is uncontrolled access to the machine as the command line and thus the key can be viewed as part of the process info. 

 * :option:`--encrypt-key-file=KEYFILE` - the name of a file where the raw key of the appropriate length can be read from. The file must be a simple binary (or text) file that contains exactly the key to be used. 

Both :option:`--encrypt-key` option  and :option:`--encrypt-key-file` option can be used to specify the encryption key.

Using the :option:`--encrypt-key` option
-----------------------------------------
Example of the innobackupex command using the :option:`--encrypt-key` should look like this ::

  $ innobackupex --encrypt=AES256 --encrypt-key="secret_key_with_the_length_of_32" /data/backups


Using the :option:`--encrypt-key-file` option
----------------------------------------------
Example of the innobackupex command using the :option:`--encrypt-key-file` should look like this ::

  $ innobackupex --encrypt=AES256 --encrypt-key-file=/data/backups/keyfile /data/backups

.. note::

  Depending on the text editor used for making the ``KEYFILE``, text file in some cases can contain the CRLF and this will cause the key size to grow and thus making it invalid. Suggested way to do this would be to create the file with: ``echo -n "secret_key_with_the_length_of_32" > /data/backups/keyfile``


Both of these examples will create a timestamped directory in :file:`/data/backups` containing the encrypted backup.

.. note:: 

  You can use the :option:`innobackupex --no-timestamp` option to override this behavior and the backup will be created in the given directory.

Optimizing the encryption process
=================================

Two new options have been introduced with the encrypted backups that can be used to speed up the encryption process. These are :option:`--encrypt-threads` and :option:`--encrypt-chunk-size`. By using the :option:`--encrypt-threads` option multiple threads can be specified to be used for encryption in parallel. Option :option:`--encrypt-chunk-size` can be used to specify the size (in bytes) of the working encryption buffer for each encryption thread (default is 64K).

Decrypting Encrypted Backups
============================

Backups can be decrypted with :ref:`xbcrypt`. Following one-liner can be used to encrypt the whole folder: ::

  $ for i in `find . -iname "*\.xbcrypt"`; do xbcrypt -d --encrypt-key-file=/root/secret_key --encrypt-algo=AES256 < $i > $(dirname $i)/$(basename $i .xbcrypt) && rm $i; done

When the files have been decrypted backup can be prepared.

Preparing Encrypted Backups
============================

After the backups have been decrypted, they can be prepared the same way as the standard full backups with the :option:`--apply-logs` option: :: 

  $ innobackupex --apply-log /data/backups/2013-03-25_10-34-04

Restoring Encrypted Backups
=============================

|innobackupex| has a :option:`--copy-back` option, which performs the restoration of a backup to the server's :term:`datadir` ::

  $ innobackupex --copy-back /path/to/BACKUP-DIR

It will copy all the data-related files back to the server's :term:`datadir`, determined by the server's :file:`my.cnf` configuration file. You should check the last line of the output for a success message::

  innobackupex: Finished copying back files.
  130201 11:08:13  innobackupex: completed OK!

Other Reading
=============

* `The Libgcrypt Reference Manual <http://www.gnupg.org/documentation/manuals/gcrypt/>`_

