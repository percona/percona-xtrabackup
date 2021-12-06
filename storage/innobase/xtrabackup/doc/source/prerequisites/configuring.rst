.. _configuring:

Configuring XtraBackup
======================

Setting options configures the *Percona XtraBackup*. These options behave
exactly like the standard |MySQL| program options: they can be specified either at
the command-line or through a configuration file like `/etc/my.cnf`.

See the :ref:`option and variable reference
<xbk_option_reference>` for details on all of the configuration options.

The *Percona XtraBackup* binary reads the ``[mysqld]`` and ``[xtrabackup]`` sections from any configuration files, in that order. This order allows the binary to read options from your existing |MySQL| installation, such as the `datadir` or certain *InnoDB* options. Specify any options to be overridden in the ``[xtrabackup]`` section. This section takes precedence since it is read later.

The *Percona XtraBackup* binary does not accept exactly the same syntax in the
`my.cnf` file as the `mysqld` server binary. For historical
reasons, the `mysqld` server binary accepts parameters with a
``--set-variable=<variable>=<value>`` syntax, which *Percona XtraBackup* does not understand. If your `my.cnf` file has these configuration directives rewrite them in the ``--variable=value`` syntax.

You are not required to put any options in your `my.cnf`. You can simply specify the options on the command-line. 

The ``target_dir`` option may be convenient to put in the ``[xtrabackup]`` section
of your `my.cnf` file. This option defines default directory where the backups are placed. In the following example, the option puts the backups in the ``/data/backups/mysql/`` directory:

.. code-block:: text

  [xtrabackup]
  target_dir = /data/backups/mysql/

This manual assumes that you do not have a file-based configuration for
*Percona XtraBackup* and will always show the command-line options. 

System Configuration and NFS Volumes
---------------------------------------

The *Percona XtraBackup* tool requires no special configuration on most systems.
However, the storage where the ``--target-dir`` is located
must behave properly when ``fsync()`` is called. In particular, we have noticed
that NFS volumes not mounted with the ``sync`` option might not really sync the
data. As a result, if you back up to an NFS volume mounted with the async
option, and then try to prepare the backup from a different server that also
mounts that volume, the data might appear to be corrupt. Use the
``sync`` mount option to avoid this problem.
