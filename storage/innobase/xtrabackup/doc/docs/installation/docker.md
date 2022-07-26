# Running Percona XtraBackup in a Docker container

Docker allows you to run applications in a lightweight unit called a
container.

You can run *Percona XtraBackup* in a Docker container without installing
the product. All required libraries are available in
the container. Being a lightweight execution environment, Docker containers
enable creating
configurations where each program runs in a separate container. You may run
*Percona Server for MySQL* in one container and *Percona XtraBackup* in
another. Docker images offer a range of options.

Create a Docker container based on a Docker image. Docker images for
Percona XtraBackup
are hosted publicly on [Docker Hub](https://hub.docker.com/r/percona/percona-xtrabackup).

```
$ sudo docker create ... percona/percona-xtrabackup --name xtrabackup ...
```

### Scope of this section

This section demonstrates how to back up data
on a Percona Server for MySQL running in another Dockers container.

## Installing Docker

Your operating system may already provide a package for *docker*. However,
the versions of Docker provided by your operating system are likely to be
outdated.

Use the installation instructions for your operating system available from
the
Docker site to set up the latest version of *docker*.

## Connecting to a Percona Server for MySQL container

Percona XtraBackup works in combination with a database server. When
running a Docker container for Percona XtraBackup, you can make
backups for a database server either installed on the host machine or
running
in a separate Docker container.

To set up a database server on a host machine or in Docker
container, follow the documentation of the supported product that you
intend to use with *Percona XtraBackup*.




**See also:**

> <a href="https://docs.docker.com/storage/volumes/">Docker volumes as 
> container persistent data storage</a>

> <a href="https://docs.docker.com/config/containers/start-containers-automatically">More 
> information about containers</a>

``` sh
$ sudo docker run -d --name percona-server-mysql \
-e MYSQL_ROOT_PASSWORD=root percona/percona-server:8.0
```

As soon as Percona Server for MySQL runs, add some data to it. Now, you are
ready to make backups with Percona XtraBackup.

**Important**

> When running Percona XtraBackup from a container and connecting to a 
> MySQLserver container, we recommend using the –user root option in the 
> Docker command.

## Creating a Docker container from Percona XtraBackup image

You can create a Docker container based on Percona XtraBackup image with
either `docker create` or the `docker run` command. `docker create`
creates a Docker container and makes it available for starting later.

Docker downloads the Percona XtraBackup image from the Docker Hub. If it
is not the first time you use the selected image, Docker uses the image
available locally.

```
$ sudo docker create --name percona-xtrabackup --volumes-from percona-server-mysql \
percona/percona-xtrabackup  \
xtrabackup --backup --datadir=/var/lib/mysql/ --target-dir=/backup \
--user=root --password=mysql
```

With parameter name you give a meaningful name to your new Docker container
so
that you could easily locate it among your other containers.

The `volumes-from` flag refers to Percona Server for MySQL and indicates
that you
intend to use the same data as the Percona Server for MySQL container.

Run the container with exactly the same parameters that were used when the
container was created:

```
$ sudo docker start -ai percona-xtrabackup
```

This command starts the *percona-xtrabackup* container, attaches to its
input/output streams, and opens an interactive shell.

The `docker run` is a shortcut command that creates a Docker container and
then immediately runs it.

```
$ sudo docker run --name percona-xtrabackup --volumes-from percona-server-mysql \
percona/percona-xtrabackup
xtrabackup --backup --data-dir=/var/lib/mysql --target-dir=/backup --user=root --password=mysql
```

<!-- Company names (e.g. Percona, Oracle), product names (e.g. MySQL, Ubuntu) -->
<!-- Commands formatted with *command* role -->
<!-- Parameters, options, and configuration variables -->
