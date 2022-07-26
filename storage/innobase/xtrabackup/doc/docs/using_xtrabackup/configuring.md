# Configuring xtrabackup

All the *xtrabackup* configuration is done through options, which behave
exactly like standard *MySQL* program options: they can be specified either at
the command-line, or through a file such as `/etc/my.cnf`.

The *xtrabackup* binary reads the `[mysqld]` and `[xtrabackup]` sections
from any configuration files, in that order. That is so that it can read its
options from your existing *MySQL* installation, such as the datadir or
some of the *InnoDB* options. If you want to override these, just specify them
in the `[xtrabackup]` section, and because it is read later, it will take
precedence.

You don’t need to put any configuration in your `my.cnf` if you don’t
want to. You can simply specify the options on the command-line. Normally, the
only thing you might find convenient to place in the `[xtrabackup]` section
of your `my.cnf` file is the `target_dir` option to default the
directory in which the backups will be placed, for example:

``` text
[xtrabackup]
target_dir = /data/backups/mysql/
```

This manual will assume that you do not have any file-based configuration for
*xtrabackup*, so it will always show command-line options being used
explicitly. Please see the option and variable reference for details on all the configuration options.

The *xtrabackup* binary does not accept exactly the same syntax in the
`my.cnf` file as the **mysqld** server binary does. For historical
reasons, the **mysqld** server binary accepts parameters with a
`--set-variable=<variable>=<value>` syntax, which *xtrabackup* does not
understand. If your `my.cnf` file has such configuration directives, you
should rewrite them in the `--variable=value` syntax.

## System Configuration and NFS Volumes

The *xtrabackup* tool requires no special configuration on most systems.
However, the storage where the `--target-dir` is located
must behave properly when `fsync()` is called. In particular, we have noticed
that if you mount the NFS volume without the `sync` option the NFS 
volume does not sync the data. As a result, if you back up to an NFS 
volume mounted with the async
option, and then try to prepare the backup from a different server that also
mounts that volume, the data might appear to be corrupt. You can use the
`sync` mount option to avoid this problem.
