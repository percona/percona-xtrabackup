.. _docker:

Run *Percona XtraBackup* in a `Docker` container
********************************************************************************

You may run Percona XtraBackup in a `Docker` container without
having to install it. All required libraries are installed in
the container.

Being a lightweight execution environment, `Docker` containers enable creating
configurations where each program runs in a separate container. You can run
Percona Server for MySQL in one container and Percona XtraBackup in another.

A `Docker` container is based on a `Docker` image, which works as a
template for newly created containers. The `Docker` images for Percona XtraBackup are hosted publicly on the ` Docker Hub <https://hub.docker.com/r/percona/percona-xtrabackup>`__.

Install `Docker`
================================================================================

The Docker engine can be installed on many platforms. Consult `Install Docker Engine <https://docs.docker.com/engine/install/>`__ for the latest versions, system requirements, and instructions.

Create and run a **Percona XtraBackup** Docker container
===============================================================================

1. Create a directory for the container. Within that directory, pull the latest image of Percona XtraBackup from Docker Hub repository. This operation may take a few minutes. 

   .. code-block:: bash

      $ docker pull percona/percona-xtrabackup

2. After the download is complete, create the container in an interactive mode. The ``-i`` option attaches the standard input stream (stdin) of the bash shell and the ``-t`` creates a terminal for the process. 

   The hostname is automatically generated when you create a container. For this example, the hostname is ``660cdc388af`` in these steps. The hostname for your container will be different.

   .. code-block:: bash

      $ docker run -it percona/percona-xtrabackup /bin/bash
      [root@e660cdc388af /]#

3. You access the container as root. 

   .. code-block:: bash

      [root@e660cdc388af /]# xtrabackup --version
      xtrabackup version 8.0.26-18 based on MySQL server 8.0.26 Linux (x86_64) (revision id: 4aecf82)

Run a container in daemon mode
==================================================

You can run a container in detached (daemon) mode to run in the background. 

.. code-block:: bash

   $ docker run -it -d percona/percona-xtrabackup bash

You can verify that the container is running with the following command:

.. code-block:: bash

   $ docker ps 
   CONTAINER ID   IMAGE                        COMMAND   CREATED              STATUS              PORTS     NAMES
   ffb00224ea9f   percona/percona-xtrabackup   "bash"    30 seconds ago       Up 29 seconds                 sleepy_maxwell

The following command uses the container ID to access the container.

.. code-block:: bash

   $ docker exec -it ffb00224ea9f bash
   [root@ffb00224ea9f /]#

You can send a command to verify that you are running in the container:

.. code-block:: bash

   [root@ffb00224ea9f /]# xtrabackup --version
   xtrabackup version 8.0.26-18 based on MySQL server 8.0.26 Linux (x86_64) (revision id: 4aecf82)


Exiting a container
=================================================

To exit and stop a container, you can use any of the following methods:

* In the container, enter ``exit``.

  .. code-block:: bash

     [root@e660cdc388af /]# exit

* The ``ctrl+d`` key combination

If you exit the container in detached mode and do not want the container to stop, at the container's prompt, enter the following key combinations:

* ``ctrl+p`` and ``ctrl+q``

.. code-block:: bash

   [root@ffb00224ea9f /]# read escape sequence
   $

Connecting to a Percona Server for MySQL container
================================================================================

Percona XtraBackup works in combination with a database server. When
running a `Docker` container for Percona XtraBackup, you can make
backups for a database server either installed on the host machine or running
in a separate `Docker` container.

To set up a database server on a host machine or in `Docker`
container, follow the documentation of the supported product that you
intend to use with Percona XtraBackup.

.. code-block:: bash

   $ sudo docker run -d --name percona-server-mysql \
   -e MYSQL_ROOT_PASSWORD=root percona/percona-server:8.0

.. seealso::

   Percona Server for MySQL Documentation:
      - `Installing on a host machine
	<https://www.percona.com/doc/percona-server/LATEST/installation.html>`_
      - `Running in a Docker container
	<https://www.percona.com/doc/percona-server/LATEST/installation/docker.html>`_


As soon as Percona Server for MySQL runs, add some data to it. Now, you are
ready to make backups with Percona XtraBackup.

Use Docker Inspect to find container information 
==================================================


The xtrabackup container needs the  host's local directory and the container's internal IP address. 

While the database server is running, type the following command:

.. code-block:: bash

   docker inspect <container name or container id>

In the results, search for "Mounts" and "Networks".

.. code-block:: json

   "Mounts": [
            {
                "Type": "volume",
                "Name": "96344bf8b0edcfd070e486e349ab6dff41869a28d8584fa57c9e5e7a77775d26",
                "Source": "/var/lib/docker/volumes/96344bf8b0edcfd070e486e349ab6dff41869a28d8584fa57c9e5e7a77775d26/_data",
                "Destination": "/var/lib/mysql",
                "Driver": "local",
                "Mode": "",
                "RW": true,
                "Propagation": ""
            },
   ...
   "Networks": {
                "bridge": {
                    "IPAMConfig": null,
                    "Links": null,
                    "Aliases": null,
                    "NetworkID": "eade3d3d51988c41192a79ba903c5bda0bc1f211ec24027fb4bb64a28aa3c00d",
                    "EndpointID": "d5d8f2233e9fe605390a8ff5542880b8b73ffe34d99aa14ae3f5011fb63951ee",
                    "Gateway": "172.17.0.1",
                    "IPAddress": "172.17.0.4",
                    "IPPrefixLen": 16,
                    "IPv6Gateway": "",
                    "GlobalIPv6Address": "",
                    "GlobalIPv6PrefixLen": 0,
                    "MacAddress": "02:42:ac:11:00:04",
                    "DriverOpts": null
                }

For this example, the source directory is ``"/var/lib/docker/volumes/96344bf8b0edcfd070e486e349ab6dff41869a28d8584fa57c9e5e7a77775d26/_data"``, and the network address is ``"172.17.0.4"``.

​​Creating a `Docker` container from Percona XtraBackup image
================================================================================

You can create a `Docker` container based on Percona XtraBackup image with
either ``docker create`` or the ``docker run`` command. ``docker create``
creates a `Docker` container and makes it available for starting later.

`Docker` downloads the Percona XtraBackup image from the Docker Hub. If it
is not the first time you use the selected image, `Docker` uses the image available locally.

.. code-block:: bash

   $ sudo docker create --name percona-xtrabackup --volumes-from percona-server-mysql \
   percona/percona-xtrabackup  \
   xtrabackup --backup --datadir=/var/lib/mysql/ --target-dir=/backup \
   --user=root --password=mysql

With parameter name you give a meaningful name to your new `Docker` container so
that you could easily locate it among your other containers.

The ``volumes-from`` flag refers to Percona Server for MySQL and indicates that you
indend to use the same data as the Percona Server for MySQL container.

Run the container with exactly the same parameters that were used when the container was created:

.. code-block:: bash

   $ sudo docker start -ai percona-xtrabackup

This command starts the *percona-xtrabackup* container, attaches to its
input/output streams, and opens an interactive shell.

The ``docker run`` is a shortcut command that creates a `Docker` container and then immediately runs it.

.. code-block:: bash

   $ sudo docker run --name percona-xtrabackup --volumes-from percona-server-mysql \
   percona/percona-xtrabackup
   xtrabackup --backup --data-dir=/var/lib/mysql --target-dir=/backup --user=root --password=mysql

.. seealso::

   More in `Docker` documentation
      - `Docker volumes as persistent data storage for containers
	<https://docs.docker.com/storage/volumes/>`_
      - `More information about containers
	<https://docs.docker.com/config/containers/start-containers-automatically/>`_

.. include:: ../_res/replace/proper.txt
.. include:: ../_res/replace/command.txt
.. include:: ../_res/replace/parameter.txt
