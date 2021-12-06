.. _connections:

================================================================================
Connections Needed
================================================================================

**Percona XtraBackup** connects to the database server to perform operations and access the ``datadir`` for the following procedures:

* Creating a backup
* Preparing a backup
* Restoring the backup

The correct privileges and permissions are required for the successful execution of these procedures.

Privileges refer to the operations that a system user is permitted to do in
the database server. Privileges are set at the database level and only apply to
database users.

Permissions let a user perform specific activities on the system,
such as reading, writing, executing on a certain directory, or starting or stopping a system service. Permissions are set at a system level and only apply to system users.

When *Percona XtraBackup* is used, there are two actors involved: the user invoking the
program - *a system user* - and the user performing action in the database
server - *a database user*. Note that these are different users in different
places, even though they may have the same username.

All the invocations of *Percona XtraBackup* in this documentation assume that the system
user has the appropriate permissions and you are providing the relevant options
for connecting the database server - besides the options for the action to be
performed - and the database user has adequate privileges.

.. _pxb.privilege.server.connecting:

Connecting to the server
================================================================================

The `--user` is the database user and `--password` option is the password used to connect to the server.

.. code-block:: bash

  $ xtrabackup --user=DVADER --password=14MY0URF4TH3R --backup \
  --target-dir=/data/bkps/

If you don't use the :option:`--user` option, |Percona XtraBackup| will assume
the database user whose name is the system user executing it.

.. _pxb.privilege.server.option.connecting:

Other Connection Options
--------------------------------------------------------------------------------

According to your system, you may need to specify one or more of the following
options to connect to the server:

===========  ==================================================================
Option       Description
===========  ==================================================================
--port       The port to use when connecting to the database server with
             TCP/IP.
--socket     The socket to use when connecting to the local database.
--host       The host to use when connecting to the database server with
             TCP/IP.
===========  ==================================================================

These options are passed to the :command:`mysql` child process without
alteration, see ``mysql --help`` for details.

.. note::

   In case of multiple server instances, the correct connection parameters
   (port, socket, host) must be specified in that particular order for *Percona XtraBackup* to communicate with the correct server.


   

