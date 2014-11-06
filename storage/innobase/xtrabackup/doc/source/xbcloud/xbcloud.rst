.. _xbcloud_binary:

======================
 The xbcloud Binary
======================

.. note::

   This feature implementation is considered **ALPHA** quality.

|xbcloud| is a new tool which is part of the |Percona XtraBackup| 2.3.0-alpha1 release. The purpose of |xbcloud| is to download and upload full or part of |xbstream| archive from/to cloud. Archive uploading will employ multipart upload for Amazon and Large Objects on Swift. Along with |xbstream| archive index file will be uploaded which contains list of files and their parts and offsets of those parts in the |xbstream| archive. This index is needed for downloading only part of archive (one or several tables from backups) on demand.

Usage
-----

Backup: ::

 innobackupex --stream=xbstream /tmp | xbcloud [options] put <name>

Following example shows how to make a full backup and upload it to Swift: :: 

 innobackupex --stream=xbstream --extra-lsndir=/tmp /tmp | \
 xbcloud put --storage=Swift \
 --swift-container=test \
 --swift-user=test:tester \
 --swift-url=http://192.168.8.80:8080/ \
 --swift-key=testing \
 --parallel=10 \
 full_backup

Restore: :: 

 xbcloud [options] get <name> [<list-of-files>] | xbstream -x

Following example shows how to fetch and restore the backup from Swift: :: 

  xbcloud get --storage=Swift \
  --swift-container=test \
  --swift-user=test:tester \
  --swift-url=http://192.168.8.80:8080/ \
  --swift-key=testing \
  full_backup | xbstream -xv -C /tmp/downloaded_full

  innobackupex --apply-log /tmp/downloaded_full
  innobackupex --copy-back /tmp/downloaded_full

Limitations
-----------

Restoring individual tables from full cloud backup isn't possible without downloading the entire backup.

Command-line options
--------------------

|xbcloud| has following command line options:

.. option:: --storage

   Cloud storage option. Only support for Swift is currently implemented. Default is Swift

.. option:: --swift-url 

   URL of Swift cluster

.. option:: --swift-user

   Swift username (X-Auth-User, specific to Swift)

.. option:: --swift-key 

   Swift key/password (X-Auth-Key, specific to Swift)

.. option:: --swift-container 

   Container to backup into (specific to Swift)

.. option:: --parallel=N 

   Maximum number of concurrent upload/download threads. Default is 1.

.. option:: --cacert 

   Path to the file with CA certificates

.. option:: --insecure 

   Do not verify servers certificate
