.. _pxc.installing/docker.running:

Running |percona-xtrabackup| in a |docker| container
********************************************************************************

You may run |percona-xtrabackup| in a |docker| container without
having to install it. All required libraries come already installed in
the container.

Being a lightweight execution environment, |docker| containers enable creating
configurations where each program runs in a separate container. You may run
|percona-server| in one container and |percona-xtrabackup| in another.

You create a new |docker| container based on a |docker| image, which works as a
template for newly created containers. |docker| images for |percona-xtrabackup|
are hosted publicly on |docker-hub| at |dockerhub.percona-xtrabackup|.

.. code-block:: bash

   $ sudo docker create ... percona/percona-xtrabackup --name xtrabackup ...

.. rubric:: Scope of this section

|docker| containers offer a range of different options effectively allowing
to create quite complex setup. This section demonstrates how to backup data
on a |percona-server-mysql| running in another |docker| container.

Installing |docker|
================================================================================

Your operating system may already provide a package for |cmd.docker|. However,
the versions of |docker| provided by your operating system are likely to be
outdated.

Use the installation instructions for your operating system available from the
|docker| site to set up the latest version of |cmd.docker|.

.. seealso::

   |Docker| Documentation:
      - `How to use Docker <https://docs.docker.com/>`_
      - `Installing <https://docs.docker.com/get-docker/>`_
      - `Getting started <https://docs.docker.com/get-started/>`_

Connecting to a |percona-server| container
================================================================================

|percona-xtrabackup| works in combination with a database server. When
running a |docker| container for |percona-xtrabackup|, you can make
backups for a database server either installed on the host machine or running
in a separate |docker| container.

To set up a database server on a host machine or in |docker|
container, follow the documentation of the supported product that you
intend to use with |percona-xtrabackup|.

.. seealso::

   |percona-server-mysql| Documentation:
      - `Installing on a host machine
	<https://www.percona.com/doc/percona-server/LATEST/installation.html>`_
      - `Running in a Docker container
	<https://www.percona.com/doc/percona-server/LATEST/installation/docker.html>`_

.. code-block:: bash

   $ sudo docker run -d --name percona-server-mysql \
   -e MYSQL_ROOT_PASSWORD=root percona/percona-server:8.0

As soon as |percona-server-mysql| runs, add some data to it. Now, you are
ready to make backups with |percona-xtrabackup|.

Creating a |docker| container from |percona-xtrabackup| image
================================================================================

You can create a |docker| container based on |percona-xtrabackup| image with
either |cmd.docker-create| or |cmd.docker-run| command. |cmd.docker-create|
creates a |docker| container and makes it available for starting later.

|docker| downloads the |percona-xtrabackup| image from the |docker-hub|. If it
is not the first time you use the selected image, |docker| uses the image available locally.

.. code-block:: bash

   $ sudo docker create --name percona-xtrabackup --volumes-from percona-server-mysql \
   percona/percona-xtrabackup  \
   xtrabackup --backup --datadir=/var/lib/mysql/ --target-dir=/backup \
   --user=root --password=mysql

With |param.name| you give a meaningful name to your new |docker| container so
that you could easily locate it among your other containers.

The |param.volumes-from| referring to *percona-server-mysql* indicates that you
indend to use the same data as the *percona-server-mysql* container.

Run the container with exactly the same parameters that were used when the container was created:

.. code-block:: bash

   $ sudo docker start -ai percona-xtrabackup

This command starts the *percona-xtrabackup* container, attaches to its
input/output streams, and opens an interactive shell.

The |cmd.docker-run| is a shortcut command that creates a |docker| container and then immediately runs it.

.. code-block:: bash

   $ sudo docker run --name percona-xtrabackup --volumes-from percona-server-mysql \
   percona/percona-xtrabackup
   xtrabackup --backup --data-dir=/var/lib/mysql --target-dir=/backup --user=root --password=mysql

.. seealso::

   More in |docker| documentation
      - `Docker volumes as persistent data storage for containers
	<https://docs.docker.com/storage/volumes/>`_
      - `More information about containers
	<https://docs.docker.com/config/containers/start-containers-automatically/>`_

.. include:: ../_res/replace/proper.txt
.. include:: ../_res/replace/command.txt
.. include:: ../_res/replace/parameter.txt
