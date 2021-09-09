.. _xbcloud_exbackoff:

==========================================
Exponential Backoff 
==========================================

This feature was implemented in :ref:`PXB-8.0.26-18.0` in the xbcloud binary.

Exponential backoff increases the chances for the completion of a backup or a restore operation. For example, a chunk upload or download may fail if you have an unstable network connection or other network issues. This feature adds an exponential backoff, or sleep, time and then retries the upload or download.

When a chunk upload or download operation fails, xbcloud checks the reason for the failure. This failure can be a CURL error or an HTTP error, or a client-specific error. If the error is listed in the :ref:`retriable` list, xbcloud pauses for a calculated time before retrying the operation until that time reaches the ``--max-backoff`` value. 

The operation is retried until the ``--max-retries`` value is reached. If the chunk operation fails on the last retry, xbcloud aborts the process.

The default values are the following:

* --max-backoff = 300000 (5 minutes)

* --max-retries = 10


You can adjust the number of retries by adding the ``--max-retries`` parameter and adjust the maximum length of time between retries by adding the ``--max-backoff`` parameter to an xbcloud command. 

Since xbcloud does multiple asynchronous requests in parallel, a calculated value, measured in milliseconds, is used for ``max-backoff``. This algorithm calculates how many milliseconds to sleep before the next retry. A number generated is based on the combining the power of two (2), the number of retries already attempted and adds a random number between 1 and 1000. This number avoids network congestion if multiple chunks have the same backoff value. If the default values are used, the final retry attempt to be approximately 17 minutes after the first try. The number is no longer calculated when the milliseconds reach the ``--max-backoff`` setting. At that point, the retries continue by using the ``--max-backoff`` setting until the ``max-retries`` parameter is reached.

.. _retriable:

Retriable errors
------------------

We retry for the following CURL operations:

* CURLE_GOT_NOTHING

* CURLE_OPERATION_TIMEOUT

* CURLE_RECV_ERROR

* CURLE_SEND_ERROR

* CURLE_SEND_FAIL_REWIND

* CURLE_PARTIAL_FILE

* CURLE_SSL_CONNECT_ERROR

We retry for the following HTTP operation status codes:

* 503

* 500

* 504

* 408

Each cloud provider may return a different CURL error or an HTTP error, depending on the issue. Add new errors by setting the following variables ``--curl-retriable-errors`` or ``--http-retriable-errors`` on the command line or in ``my.cnf`` or in a custom configuration file under the [xbcloud] section.

The error handling is enhanced when using the ``--verbose`` output. This output specifies which error caused xbcloud to fail and what parameter a user must add to retry on this error. 

The following is an example of a verbose output:

.. sourcecode:: bash

    210701 14:34:23 /work/pxb/ins/8.0/bin/xbcloud: Operation failed. Error: Server returned nothing (no headers, no data)
    210701 14:34:23 /work/pxb/ins/8.0/bin/xbcloud: Curl error (52) Server returned nothing (no headers, no data) is not configured as retriable. You can allow it by adding --curl-retriable-errors=52 parameter

Example
--------
The following example adjusts the maximum number of retries and the maximum time between retries.

.. sourcecode:: bash

     xbcloud [options] --max-retries=5 --max-backoff=10000

The following text is an example of the exponential backoff used with the command:

.. sourcecode:: bash

    210702 10:07:05 /work/pxb/ins/8.0/bin/xbcloud: Operation failed. Error: Server returned nothing (no headers, no data)
    210702 10:07:05 /work/pxb/ins/8.0/bin/xbcloud: Sleeping for 2384 ms before retrying backup3/xtrabackup_logfile.00000000000000000006 [1]
    . . .
    210702 10:07:23 /work/pxb/ins/8.0/bin/xbcloud: Operation failed. Error: Server returned nothing (no headers, no data)
    210702 10:07:23 /work/pxb/ins/8.0/bin/xbcloud: Sleeping for 4387 ms before retrying backup3/xtrabackup_logfile.00000000000000000006 [2]
    . . .
    210702 10:07:52 /work/pxb/ins/8.0/bin/xbcloud: Operation failed. Error: Failed sending data to the peer
    210702 10:07:52 /work/pxb/ins/8.0/bin/xbcloud: Sleeping for 8691 ms before retrying backup3/xtrabackup_logfile.00000000000000000006 [3]
    . . .
    210702 10:08:47 /work/pxb/ins/8.0/bin/xbcloud: Operation failed. Error: Failed sending data to the peer
    210702 10:08:47 /work/pxb/ins/8.0/bin/xbcloud: Sleeping for 10000 ms before retrying backup3/xtrabackup_logfile.00000000000000000006 [4]
    . . .
    210702 10:10:12 /work/pxb/ins/8.0/bin/xbcloud: successfully uploaded chunk: backup3/xtrabackup_logfile.00000000000000000006, size: 8388660

The following list details the example output:

    [1.] Chunk ``xtrabackup_logfile.00000000000000000006`` fails to upload _ the first time and slept for 2384 milliseconds.

    [2.] The same chunk fails for the second time and the time is increased to 4387 milliseconds. 

    [3.] The same chunk fails for the third time and the time is increased to 8691 milliseconds.

    [4.] The same chunk fails for the fourth time. The ``max-backoff`` parameter has been reached. All retries sleep the same amount of time after reaching the parameter.

    [5.] The same chunk is successfully uploaded.

