.. _keyring_vault_support:

=============================================
Keyring support with the keyring_vault plugin
=============================================

Introduction
------------

|xtrabackup| comes with two keyring plugins - ``keyring_file`` and
``keyring_vault``. These plugins are installed into the <plugin directory>.

When doing a backup, |xtrabackup| contacts server to determine which
keyring plguin should be used and what are the settings.

Usage
-----

Command like::

  xtrabackup --backup -uroot -p --target-dir=./backup1

will create a backup in the ``./backup1`` directory. To prepare this
backup, |xtrabackup| will need an access to the keyring. Since
xtrabackup doesn't talk to MySQL server and doesn't read default
``my.cnf`` configuration file during prepare, one will need to specify keyring
settings via the command line::

  xtrabackup --prepare --target-dir=./backup1 --keyring-vault-config=/etc/vault.cnf

Copy-back works as usual::

  xtrabackup --copy-back --target-dir=./backup1 --datadir=/data/mysql

While this method works, it requires an access to the same keyring
which server is using. It may not be possible if backup prepared on
different server or at the much later time, when keys in the keyring
have been purged, or in case of malfunction when keyring server is not
available at all.

A ``--transition-key=<passphrase>`` option should be used to make it possible
for |xtrabackup| to prepare the backup without access to the keyring vault
server. In this case |xtrabackup| will derive AES encryption
key from specified passphrase and will use it to encrypt tablespace keys
of tablespaces being backed up.

Here is an example::

  xtrabackup --backup -uroot -p --transition-key=MySecetKey --target-dir=./backup2

If ``--transition-key`` is specified without a value, xtrabackup will ask for it.

.. note: |xtrabackup| scrapes ``--transition-key`` so that its value is not
   visible in the ``ps`` command output.

The same passphrase need to be specified for prepare command::

  xtrabackup --prepare --target-dir=./backup2

There is no -keyring-vault... options, because
|xtrabackup| does not talk to vault server in this case.

When restoring a backup you will need to generate new master
key. For example::

  xtrabackup --copy-back --target-dir=./backup1 --datadir=/data/mysql --transition-key=MySecetKey --generate-new-master-key --keyring-vault-config=/etc/vault.cnf

|xtrabackup| will generate new master key, store it into target keyring
vault server and re-encrypt tablespace keys using this key.

Finally, there is an option to store transition key on the keyring
vault server. In this case |xtrabackup| will need an access to the same
vault server during prepare and copy-back, but does not depend on
whether the server keys have been purged.

Backup::

  xtrabackup --backup -uroot -p --generate-transition-key --target-dir=./backup2

Prepare::

  xtrabackup --prepare --target-dir=./backup1 --keyring-vault-config=/etc/vault.cnf

Copy-back::

  xtrabackup --copy-back --target-dir=./backup1 --datadir=/data/mysql --generate-new-master-key --keyring-vault-config=/etc/vault.cnf

Keyring vault plugin settings are described `here <https://www.percona.com/doc/percona-server/LATEST/management/data_at_rest_encryption.html>'_.
