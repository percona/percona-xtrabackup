.. _xbcloud_binary:

================================================================================
The xbcloud Binary
================================================================================

:Availability: This feature implementation is considered **ALPHA** quality

|xbcloud| is a new tool which is part of the |Percona XtraBackup| 2.3.2
release. The purpose of |xbcloud| is to download and upload full or part of
|xbstream| archive from/to cloud. |xbcloud| will refuse to overwrite the backup
with the same name.

|xbcloud| stores each chunk as a separate object with name
``backup_name/database/table.ibd.NNNNNNNNNNNNNNNNNNNN``, where ``NNN...`` is a
0-padded serial number of chunk within file. Size of chunk produced by
|xtrabackup| and |xbstream| changed to 10M.

Version specific information
--------------------------------------------------------------------------------

- 2.3.0-alpha1 - Initial implementation
- 2.3.1-beta1 - Implemented ability to store *xbcloud* parameters in a
  :file:`.cnf` file
- 2.3.1-beta1 - Implemented support different :ref:`authentication options
  <swift_auth>` for Swift
- 2.3.1-beta1 - Implemented support for partial download of the cloud backups
- 2.3.1-beta1 - :option:`xbcloud --swift-url` option has been renamed to
  :option:`xbcloud --swift-auth-url`

Usage
--------------------------------------------------------------------------------

.. rubric:: Backup:

.. code-block:: bash

   $ xtrabackup --backup --stream=xbstream --target-dir=/tmp | xbcloud \
   put [options] <name>

The following example shows how to make a full backup and upload it to Swift:

.. code-block:: bash

   $ xtrabackup --backup --stream=xbstream --extra-lsndir=/tmp --target-dir=/tmp | \
   xbcloud put --storage=Swift \
   --swift-container=test \
   --swift-user=test:tester \
   --swift-auth-url=http://192.168.8.80:8080/ \
   --swift-key=testing \
   --parallel=10 \
   full_backup

.. rubric:: Restore:

.. code-block:: bash

   $ xbcloud get [options] <name> [<list-of-files>] | xbstream -x

Following example shows how to fetch and restore the backup from Swift:

.. code-block:: bash

   $ xbcloud get --storage=Swift \
   --swift-container=test \
   --swift-user=test:tester \
   --swift-auth-url=http://192.168.8.80:8080/ \
   --swift-key=testing \
   full_backup | xbstream -xv -C /tmp/downloaded_full

   $ xtrabackup --prepare --target-dir=/tmp/downloaded_full
   $ xtrabackup --copy-back --target-dir=/tmp/downloaded_full

Incremental backups
--------------------------------------------------------------------------------

.. rubric:: Taking incremental backups:

First you need to make the full backup on which the incremental one is going to
be based:

.. code-block:: bash

   $ xtrabackup --backup --stream=xbstream --extra-lsndir=/storage/backups/ \
   --target-dir=/storage/backups/ | xbcloud put \
   --storage=swift --swift-container=test_backup \
   --swift-auth-version=2.0 --swift-user=admin \
   --swift-tenant=admin --swift-password=xoxoxoxo \
   --swift-auth-url=http://127.0.0.1:35357/ --parallel=10 \
   full_backup

Then you can make the incremental backup:

.. code-block:: bash

   $ xtrabackup --backup --incremental-basedir=/storage/backups \
   --stream=xbstream --target-dir=/storage/inc_backup | xbcloud put \
   --storage=swift --swift-container=test_backup \
   --swift-auth-version=2.0 --swift-user=admin \
   --swift-tenant=admin --swift-password=xoxoxoxo \
   --swift-auth-url=http://127.0.0.1:35357/ --parallel=10 \
   inc_backup

.. rubric:: Preparing incremental backups:

To prepare the backup you first need to download the full backup:

.. code-block:: bash

   $ xbcloud get --swift-container=test_backup \
   --swift-auth-version=2.0 --swift-user=admin \
   --swift-tenant=admin --swift-password=xoxoxoxo \
   --swift-auth-url=http://127.0.0.1:35357/ --parallel=10 \
   full_backup | xbstream -xv -C /storage/downloaded_full

Once you download full backup it should be prepared:

.. code-block:: bash

   $ xtrabackup --prepare --apply-log-only --target-dir=/storage/downloaded_full

After the full backup has been prepared you can download the incremental backup:

.. code-block:: bash

   $ xbcloud get --swift-container=test_backup \
   --swift-auth-version=2.0 --swift-user=admin \
   --swift-tenant=admin --swift-password=xoxoxoxo \
   --swift-auth-url=http://127.0.0.1:35357/ --parallel=10 \
   inc_backup | xbstream -xv -C /storage/downloaded_inc

Once the incremental backup has been downloaded you can prepare it by running:

.. code-block:: bash

   $ xtrabackup --prepare --apply-log-only \
   --target-dir=/storage/downloaded_full \
   --incremental-dir=/storage/downloaded_inc

   $ xtrabackup --prepare --target-dir=/storage/downloaded_full

Partial download of the cloud backup
--------------------------------------------------------------------------------

If you don't want to download entire backup to restore the specific database
you can specify only tables you want to restore:

.. code-block:: bash

   $ xbcloud get --swift-container=test_backup
   --swift-auth-version=2.0 --swift-user=admin \
   --swift-tenant=admin --swift-password=xoxoxoxo \
   --swift-auth-url=http://127.0.0.1:35357/ full_backup \
   ibdata1 sakila/payment.ibd \
   > /storage/partial/partial.xbs

   $ xbstream -xv -C /storage/partial < /storage/partial/partial.xbs

This command will download just ``ibdata1`` and ``sakila/payment.ibd`` table
from the full backup.

Command-line options
--------------------------------------------------------------------------------

|xbcloud| has following command line options:

.. program:: xbcloud

.. option:: --storage

   Cloud storage option. Only support for Swift is currently implemented.
   Default is ``Swift``

.. option:: --swift-auth-url

   URL of Swift cluster.

.. option:: --swift-url

   Renamed to :option:`xbcloud --swift-auth-url`

.. option:: --swift-storage-url

   xbcloud will try to get object-store URL for given region (if any specified)
   from the keystone response. One can override that URL by passing
   --swift-storage-url=URL argument.

.. option:: --swift-user

   Swift username (X-Auth-User, specific to Swift)

.. option:: --swift-key

   Swift key/password (X-Auth-Key, specific to Swift)

.. option:: --swift-container

   Container to backup into (specific to Swift)

.. option:: --parallel=N

   Maximum number of concurrent upload/download threads. Default is ``1``.

.. option:: --cacert

   Path to the file with CA certificates

.. option:: --insecure

   Do not verify servers certificate

.. _swift_auth:

Swift authentication options
--------------------------------------------------------------------------------

Swift specification describe several `authentication options
<http://docs.openstack.org/developer/swift/overview_auth.html>`_. |xbcloud| can
authenticate against keystone with API version 2 and 3.

.. option:: --swift-auth-version

   Specifies the swift authentication version. Possible values are: ``1.0`` -
   TempAuth, ``2.0`` - Keystone v2.0, and ``3`` - Keystone v3. Default value is
   ``1.0``.

For v2 additional options are:

.. option:: --swift-tenant

   Swift tenant name.

.. option:: --swift-tenant-id

   Swift tenant ID.

.. option:: --swift-region

   Swift endpoint region.

.. option:: --swift-password

   Swift password for the user.

For v3 additional options are:

.. option:: --swift-user-id

   Swift user ID.

.. option:: --swift-project

   Swift project name.

.. option:: --swift-project-id

   Swift project ID.

.. option:: --swift-domain

   Swift domain name.

.. option:: --swift-domain-id

   Swift domain ID.

