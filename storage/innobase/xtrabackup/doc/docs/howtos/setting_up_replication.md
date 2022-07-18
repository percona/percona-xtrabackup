# How to setup a replica for replication in 6 simple steps with Percona XtraBackup

Data is, by far, the most valuable part of a system. Having a backup done
systematically and available for a rapid recovery in case of failure is
admittedly essential to a system. However, it is not common practice because of
its costs, infrastructure needed or even the boredom associated to the
task. *Percona XtraBackup* is designed to solve this problem.

You can have almost real-time backups in 6 simple steps by setting up a
replication environment with *Percona XtraBackup*.

*Percona XtraBackup* is a tool for backing up your data
without interruption. It performs “hot backups” on unmodified versions of
*MySQL* servers (5.1, 5.5 and 5.6), as well as *MariaDB* and *Percona
Server for MySQL*. It is a totally free and open source software distributed only under
the *GPLv2* license.

## All the things you will need

Setting up a replica for replication with *Percona XtraBackup* is a
straightforward procedure. You must have the following things to complete the steps:

* `Source` A system with a *MySQL*-based server installed, configured and
running. This system is called the `Source`, and is where your data is
stored and used for replication. We assume the following about this
server:

    * Communication enabled with others by the standard TCP/IP port

    * Installed and configured *SSH* server

    * Configured user account in the system with the appropriate permissions and privileges

    * Enabled binlogs and the server-id set up to 1

* `Replica` *MySQL*-based server installed on another server. This server is
called the `Replica`. We assume the same configuration as the `Source`,
except that the server-id on the `Replica` is 2.

* `Percona XtraBackup` The backup tool should be installed in
both servers for convenience.

!!! note

    It is not recommended to mix MySQL variants (Percona Server, MySQL, MariaDB) in your replication setup.  This may produce incorrect `xtrabackup_slave_info` file when adding a new replica.

## STEP 1: Make a backup on the `Source` and prepare it

On the `Source`, issue the following command to a shell:

```console
$ xtrabackup --backup --user=yourDBuser --password=MaGiCdB1 --target-dir=/path/to/backupdir
```

After this is finished you should see:

```console
xtrabackup: completed OK!
```

This action copies your *MySQL* data dir to the `/path/to/backupdir`
directory. You have told *Percona XtraBackup* to connect to the database server
using your database user and password, and do a hot backup of all your data in
it (all *MyISAM*, *InnoDB* tables and indexes in them).

On the `Source`, to make the snapshot consistent, prepare the data:

```console
$ xtrabackup --user=yourDBuser --password=MaGiCdB1 \
            --prepare --target-dir=/path/to/backupdir
```

Select the path where your snapshot has been taken. If everything is ok
you should see the same OK message.  Now the transaction logs are applied to the
data files, and new ones are created: your data files are ready to be used by
the MySQL server.

*Percona XtraBackup* knows where your data is by reading your my.cnf.
If you have your configuration file in a non-standard place, use the
flag `xtrabackup --defaults-file` `=/location/of/my.cnf`.

If you want to skip writing the user name and password every time you want to access *MySQL*, set the information in `.mylogin.cnf` as follows:

```default
mysql_config_editor set --login-path=client --host=localhost --user=root --password
```

This setting provides root access to MySQL.

!!! seealso

    *MySQL* Documentaiton: [MySQL Configuration Utility](https://dev.mysql.com/doc/refman/5.7/en/mysql-config-editor.html)

## STEP 2:  Copy backed up data to the `Replica`

Use `rsync` or `scp` to copy the data from Source to Replica. If you’re
syncing the data directly to replica’s data directory it’s advised to stop the
mysqld there. On the `Source`, run the following command:

```console
$ rsync -avpP -e ssh /path/to/backupdir Replica:/path/to/mysql/
```

After data has been copied you can back up the original or previously installed
*MySQL* `datadir` (**NOTE**: Make sure mysqld is shut down before you move
the contents of its datadir, or move the snapshot into its datadir.). Run the following command on the `Replica`:

```console
$ mv /path/to/mysql/datadir /path/to/mysql/datadir_bak
```

and, on the `Replica`, move the snapshot from the `Source` in its place:

```console
$ xtrabackup --move-back --target-dir=/path/to/mysql/backupdir
```

After you have copied data to the `Replica`, make sure the `Replica` *MySQL* has the proper permissions:

```console
$ chown mysql:mysql /path/to/mysql/datadir
```

In case the ibdata and iblog files are located in different directories outside
of the datadir, put them in their proper place after the logs have been applied.

## STEP 3: Configure the `Source` MySQL server

On the `Source`, add the appropriate grant to allow the replica to connect to the source:

```bash
 > GRANT REPLICATION SLAVE ON *.*  TO 'repl'@'$replicaip'
IDENTIFIED BY '$replicapass';
```

Also make sure that firewall rules are correct and that the `Replica` can connect
to the `Source`. Test that you can run the mysql client on the `Replica`,
connect to the `Source`, and authenticate.

```default
$ mysql --host=Source --user=repl --password=$replicapass
```

Verify the privileges.

```default
mysql> SHOW GRANTS;
```

## STEP 4: Configure the `Replica` MySQL server

Copy the `my.cnf` file from the `Source` to the `Replica`. On the `Replica`, run the following:

```console
$ scp user@Source:/etc/mysql/my.cnf /etc/mysql/my.cnf
```

then change the following options in /etc/mysql/my.cnf:

```console
server-id=2
```

and start/restart `mysqld` on the `Replica`.

In case you’re using init script on Debian based system to start mysqld, be sure
that the password for `debian-sys-maint` user has been updated and is the
same as the user’s password on the `Source`. This password can be seen and
updated in `/etc/mysql/debian.cnf`.

## STEP 5: Start the replication

On the `Replica`, look at the content of the file `xtrabackup_binlog_info`, it will be something like:

```console
 $ cat /var/lib/mysql/xtrabackup_binlog_info
Source-bin.000001     481
```

Execute the `CHANGE MASTER` statement on a MySQL console and use the username and password you’ve set up in STEP 3:

```mysql
TheSlave*mysql> CHANGE MASTER TO
                MASTER_HOST='$sourceip',
                MASTER_USER='repl',
                MASTER_PASSWORD='$replicapass',
                MASTER_LOG_FILE='Source-bin.000001',
                MASTER_LOG_POS=481;
```

and start the replica:

```mysql
> START SLAVE;
```

## STEP 6: Check

On the `Replica`, check that everything went OK with:

```text
TheSlave|mysql> SHOW SLAVE STATUS \G
         ...
         Slave_IO_Running: Yes
         Slave_SQL_Running: Yes
         ...
         Seconds_Behind_Master: 13
         ...
```

Both `IO` and `SQL` threads need to be running. The
`Seconds_Behind_Master` means the `SQL` currently being executed has a
`current_timestamp` of 13 seconds ago. It is an estimation of the lag between the
`Source` and the `Replica`. Note that at the beginning, a high value could
be shown because the `Replica` has to “catch up” with the
`Source`.

## Adding more replicas to the Source

You can use this procedure with slight variation to add new replicas to a
source. We will use *Percona XtraBackup* to clone an already configured
replica. We will continue using the previous scenario for convenience but we will
add the `NewReplica` to the plot.

At the `Replica`, do a full backup:

```console
 $ xtrabackup --user=yourDBuser --password=MaGiCiGaM \
--backup --slave-info --target-dir=/path/to/backupdir
```

By using the `xtrabackup --slave-info` *Percona XtraBackup* creates
additional file called xtrabackup_slave_info.

On the `Replica`, apply the logs:

```console
$ xtrabackup --prepare --use-memory=2G --target-dir=/path/to/backupdir/
```

Copy the directory from the `Replica` to the `NewReplica` (**NOTE**: Make sure
mysqld is shut down on the `NewReplica` before you copy the contents the snapshot
into its `datadir`.):

```console
rsync -avprP -e ssh /path/to/backupdir NewReplica:/path/to/mysql/datadir
```

On the `Source`, add additional grant on the source:

```mysql
> GRANT REPLICATION SLAVE ON *.*  TO 'repl'@'$newreplicaip'
                 IDENTIFIED BY '$replicapass';
```

Copy the configuration file from the `Replica`. On the `NewReplica`, run the following command:

```console
$ scp user@Replica:/etc/mysql/my.cnf /etc/mysql/my.cnf
```

Make sure you change the server-id variable in `/etc/mysql/my.cnf` to 3
and disable the replication on start:

```console
skip-slave-start
server-id=3
```

After setting `server_id`, start `mysqld`.

Fetch the master_log_file and master_log_pos from the file
`xtrabackup_slave_info`, execute the statement for setting up the source
and the log file for the `NewReplica`:

```mysql
TheNEWSlave|mysql> CHANGE MASTER TO
                   MASTER_HOST='$sourceip',
                   MASTER_USER='repl',
                   MASTER_PASSWORD='$replicapass',
                   MASTER_LOG_FILE='Source-bin.000001',
                   MASTER_LOG_POS=481;
```

and start the replica:

```mysql
> START SLAVE;
```

If both IO and SQL threads are running when you check the the `NewReplica`,
server is replicating the `Source`.
