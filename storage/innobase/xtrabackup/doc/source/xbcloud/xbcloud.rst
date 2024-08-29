.. _xbcloud_binary:

================================================================================
The xbcloud Binary
================================================================================

The purpose of *xbcloud* is to download from the cloud and upload to the cloud the full or part of an *xbstream* archive. *xbcloud* will not overwrite the backup with the same name. *xbcloud* accepts input via a pipe from *xbstream* so that it can be
invoked as a pipeline with *xtrabackup* to stream directly to the cloud without
needing a local storage.

.. note::

   In a Bash shell, the ``$?`` parameter returns the exit code from the last binary. If you use pipes, the ``${PIPESTATUS[x]}`` array parameter returns the exit codes for each binary in the pipe string. 

   .. code-block::

      $ xtrabackup --backup --stream=xbstream --target-dir=/storage/backups/ | xbcloud put [options] full_backup
       ...
      $ ${PIPESTATUS[x]}
      0 0 
      $ true | false
      $ echo $?
      1

      # with PIPESTATUS
      $ true | false
      $ echo ${PIPESTATUS[0]} ${PIPESTATUS[1]}
      0 1

The *xbcloud* binary stores each chunk as a separate object with a name
``backup_name/database/table.ibd.NNNNNNNNNNNNNNNNNNNN``, where ``NNN...`` is a
0-padded serial number of chunk within a file. Size of chunk produced by
*xtrabackup* and *xbstream* changed to 10M.

*xbcloud* has three essential operations: *put*, *get*, and *delete*. With these
operations, backups are created, stored, retrieved, restored, and
deleted. *xbcloud* operations clearly map to similar operations within the AWS
Amazon Amazon S3 API.

The :ref:`xbcloud_exbackoff` feature was implemented in Percona XtraBackup 8.0.26-18. Suppose a chunk fails to upload or download. In that case, this feature adds an exponential backoff, or sleep, time and then retries the upload or download, which increases the chances of completing a backup or a restore operation. 

Supported Cloud Storage Types
================================================================================

The following cloud storage types are supported:

* OpenStack Object Storage (Swift) - see :ref:`xbcloud_swift`

* Amazon Simple Storage (S3) - see :ref:`xbcloud_s3`

* Azure Cloud Storage - see :ref:`xbcloud_azure`

* Google Cloud Storage (gcs) - see :ref:`xbcloud_gcs`

* MinIO - see :ref:`xbcloud_minio`

In addition to OpenStack Object Storage (Swift), which has been the only option for storing backups in a cloud storage until Percona XtraBackup 2.4.14, *xbcloud* supports Amazon S3, MinIO, and Google Cloud Storage. Other Amazon S3-compatible storages, such as Wasabi or Digital Ocean Spaces, are also supported.

.. seealso::

   OpenStack Object Storage ("Swift")
      https://wiki.openstack.org/wiki/Swift
   Amazon Simple Storage Service
      https://aws.amazon.com/s3/
   MinIO
      https://min.io/
   Google Cloud Storage
      https://cloud.google.com/storage/
   Wasabi
      https://wasabi.com/
   Digital Ocean Spaces
      https://www.digitalocean.com/products/spaces/

Usage
================================================================================

The following sample command creates a full backup: 

.. code-block:: bash

   xtrabackup --backup --stream=xbstream --target-dir=/storage/backups/ --extra-lsndirk=/storage/backups/| xbcloud \
   put [options] full_backup

An incremental backup only includes the changes since the last backup. The last backup can be either a full or incremental backup. 

The following sample command creates an incremental backup:

.. sourcecode:: bash

   xtrabackup --backup --stream=xbstream --incremental-basedir=/storage/backups \
   --target-dir=/storage/inc-backup | xbcloud  put [options] inc_backup

To prepare an incremental backup, you must first download the full backup with the following command:

.. sourcecode:: bash

   xtrabackup get [options] full_backup | xbstream -xv -C /tmp/full-backup

You must prepare the full backup:

.. sourcecode:: bash

   xtrabackup --prepare --apply-log-only --target-dir=/tmp/full-backup

After the full backup has been prepared, download the incremental backup:

.. sourcecode:: bash

   xbcloud get [options] inc_backup | xbstream -xv -C /tmp/inc-backup

The downloaded backup is prepared by running the following command:

.. sourcecode:: bash

   xtrabackup --prepare --target-dir=/tmp/full-backup --incremental-dir=/tmp/inc-backup
   
You do not need the full backup to restore only a specific database. You can specify only the tables to be restored:

.. sourcecode:: bash

   xbcloud get [options] ibdata1 sakila/payment.ibd /tmp/partial/partial.xbs

   xbstream -xv -C /tmp/partial < /tmp/partial/partial.xbs



Supplying parameters
================================================================================

Each storage type has mandatory parameters that you can supply on the command
line, in a configuration file, and via environment variables.

Configuration files
--------------------------------------------------------------------------------

The parameters the values of which do not change frequently can be stored in
:file:`my.cnf` or in a custom configuration file. The following example is a
template of configuration options under the ``[xbcloud]`` group:

.. code-block:: text

   [xbcloud]
   storage=s3
   s3-endpoint=http://localhost:9000/
   s3-access-key=minio
   s3-secret-key=minio123
   s3-bucket=backupsx
   s3-bucket-lookup=path
   s3-api-version=4

.. note::

   If you explicitly use a parameter on the command line and in a configuration
   file, *xbcloud* uses the the value provided on the command line.

Environment variables
--------------------------------------------------------------------------------

If you explicitly use a parameter on the command line, in a configuration
file, and the corresponding environment variable contains a value, *xbcloud*
uses the the value provided on the command line or in the configuration file.

Shortcuts
--------------------------------------------------------------------------------

For all operations (put, get, and delete), you can use a shortcut to specify the
storage type, bucket name, and backup name as one parameter instead of using
three distinct parameters (--storage, --s3-bucket, and backup name per se).

.. admonition:: Using a shortcut syntax to provide a storage type, bucket, and backup name

   Use the following format: ``storage-type://bucket-name/backup-name``

   .. code-block:: bash

      $ xbcloud get s3://operator-testing/bak22 ...

   In this example, **s3** refers to a storage type, **operator-testing** is a
   bucket name, and **bak22** is the backup name. This shortcut expands as
   follows:

   .. code-block:: bash

      $ xbcloud get --storage=s3 --s3-bucket=operator-testing bak22 ...

You can supply the mandatory parameters not only on the command line. You may use
configuration files and environment variables.

Additional parameters
--------------------------------------------------------------------------------

*xbcloud* accepts additional parameters that you can use with any storage
type. The ``--md5`` parameter computes the MD5 hash value of the backup
chunks. The result is stored in files that following the ``backup_name.md5``
pattern.

.. code-block:: bash

   $ xtrabackup --backup --stream=xbstream \
   --parallel=8 2>backup.log | xbcloud put s3://operator-testing/bak22 \
   --parallel=8 --md5 2>upload.log

You may use the ``--header`` parameter to pass an additional HTTP
header with the server side encryption while specifying a customer key.

.. admonition:: Example of using ``--header`` for AES256 encryption

   .. code-block:: bash

      $ xtrabackup --backup --stream=xbstream --parallel=4 | \
      xbcloud put s3://operator-testing/bak-enc/ \
      --header="X-Amz-Server-Side-Encryption-Customer-Algorithm: AES256" \
      --header="X-Amz-Server-Side-Encryption-Customer-Key: CuStoMerKey=" \
      --header="X-Amz-Server-Side-Encryption-Customer-Key-MD5: CuStoMerKeyMd5==" \
      --parallel=8

The ``--header`` parameter is also useful to set the access control list (ACL)
permissions: ``--header="x-amz-acl: bucket-owner-full-control``





Incremental backups
================================================================================

First, you need to make the full backup on which the incremental one is going to
be based:

.. code-block:: bash

   xtrabackup --backup --stream=xbstream --extra-lsndir=/storage/backups/ \
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

Preparing incremental backups
--------------------------------------------------------------------------------

To prepare a backup you first need to download the full backup:

.. code-block:: bash

   $ xbcloud get --swift-container=test_backup \
   --swift-auth-version=2.0 --swift-user=admin \
   --swift-tenant=admin --swift-password=xoxoxoxo \
   --swift-auth-url=http://127.0.0.1:35357/ --parallel=10 \
   full_backup | xbstream -xv -C /storage/downloaded_full

Once you download the full backup it should be prepared:

.. code-block:: bash

   $ xtrabackup --prepare --apply-log-only --target-dir=/storage/downloaded_full

After the full backup has been prepared you can download the incremental
backup:

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

If you do not want to download the entire backup to restore the specific
database you can specify only the tables you want to restore:

.. code-block:: bash

   $ xbcloud get --swift-container=test_backup
   --swift-auth-version=2.0 --swift-user=admin \
   --swift-tenant=admin --swift-password=xoxoxoxo \
   --swift-auth-url=http://127.0.0.1:35357/ full_backup \
   ibdata1 sakila/payment.ibd \
   > /storage/partial/partial.xbs
 
   $ xbstream -xv -C /storage/partial < /storage/partial/partial.xbs
 








