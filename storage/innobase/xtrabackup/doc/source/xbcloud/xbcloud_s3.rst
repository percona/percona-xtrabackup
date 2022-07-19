.. _xbcloud_s3:

====================================================
Using xbcloud Binary with Amazon S3
====================================================

Creating a full backup with Amazon S3
================================================================================

.. code-block:: bash

   $ xtrabackup --backup --stream=xbstream --extra-lsndir=/tmp --target-dir=/tmp | \
   xbcloud put --storage=s3 \
   --s3-endpoint='s3.amazonaws.com' \
   --s3-access-key='YOUR-ACCESSKEYID' \
   --s3-secret-key='YOUR-SECRETACCESSKEY' \
   --s3-bucket='mysql_backups'
   --parallel=10 \
   $(date -I)-full_backup


The following options are available when using Amazon S3:

.. list-table::
   :header-rows: 1

   * - Option
     - Details
   * - --s3-access-key
     - Use to supply the AWS access key ID
   * - --s3-secret-key
     - Use to supply the AWS secret access key
   * - --s3-bucket
     - Use supply the AWS bucket name
   * - --s3-region
     - Use to specify the AWS region. The default value is **us-east-1**
   * - --s3-api-version = <AUTO|2|4>
     - Select the signing algorithm. The default value is AUTO. In this case, *xbcloud* will probe.
   * - --s3-bucket-lookup = <AUTO|PATH|DNS>
     - Specify whether to use **bucket.endpoint.com** or *endpoint.com/bucket**
       style requests. The default value is AUTO. In this case, *xbcloud* will probe.
   * - --s3-storage-class=<name>
     - Specify the `S3 storage class <https://docs.aws.amazon.com/AmazonS3/latest/dev/storage-class-intro.html>`__. The default storage class depends on the provider. The name options are the following: 
     
       * STANDARD
       * STANDARD_IA
       * GLACIER
       
       .. note:: 

           If you use the GLACIER storage class, the object must be `restored to S3 <https://docs.aws.amazon.com/AmazonS3/latest/dev/restoring-objects.html>`__ before restoring the backup. Also supports using custom S3 implementations such as MinIO or CephRadosGW.


Environment variables
=========================

The following environment variables are recognized. xbcloud maps them
automatically to corresponding parameters applicable to the selected storage.

- AWS_ACCESS_KEY_ID (or ACCESS_KEY_ID)
- AWS_SECRET_ACCESS_KEY (or SECRET_ACCESS_KEY)
- AWS_DEFAULT_REGION (or DEFAULT_REGION)
- AWS_ENDPOINT (or ENDPOINT)
- AWS_CA_BUNDLE

Restoring with S3
================================================================================

.. code-block:: bash

   $ xbcloud get s3://operator-testing/bak22 \
   --s3-endpoint=https://storage.googleapis.com/ \
   --parallel=10 2>download.log | xbstream -x -C restore --parallel=8
