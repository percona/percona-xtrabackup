.. _xbcloud_minio:

=================================================
Using the xbcloud Binary with MinIO
=================================================

Creating a full backup with MinIO
==================================
	    
.. code-block:: bash

   $ xtrabackup --backup --stream=xbstream --extra-lsndir=/tmp --target-dir=/tmp | \
   xbcloud put --storage=s3 \
   --s3-endpoint='play.minio.io:9000' \
   --s3-access-key='YOUR-ACCESSKEYID' \
   --s3-secret-key='YOUR-SECRETACCESSKEY' \
   --s3-bucket='mysql_backups'
   --parallel=10 \
   $(date -I)-full_backup