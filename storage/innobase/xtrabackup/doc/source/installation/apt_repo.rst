.. _apt_repo:

==========================================================
 Installing |Percona XtraBackup| on *Debian* and *Ubuntu*
==========================================================

Ready-to-use packages are available from the |Percona XtraBackup| software repositories and the `download page <https://www.percona.com/downloads/XtraBackup/>`_.

Supported Releases:

* Debian:

 * 6.0 (squeeze)
 * 7.0 (wheezy)
 * 8.0 (jessie)

* Ubuntu:

 * 12.04LTS (precise)
 * 14.04LTS (trusty)
 * 15.04 (vivid)
 * 15.10 (wily)

Supported Platforms:

* x86
* x86_64 (also known as ``amd64``)

What's in each DEB package?
===========================

The ``percona-xtrabackup`` package contains the latest |Percona XtraBackup| GA binaries and associated files.

The ``percona-xtrabackup-dbg`` package contains the debug symbols for binaries in ``percona-xtrabackup``.

The ``percona-xtrabackup-test`` package contains the test suite for |Percona XtraBackup|.

The ``percona-xtrabackup-2x`` package contains the older version of the |Percona XtraBackup|.

Installing |Percona XtraBackup| from Percona ``apt`` repository
===============================================================

1. Import the public key for the package management system

   *Debian* and *Ubuntu* packages from *Percona* are signed with the Percona's GPG key. Before using the repository, you should add the key to :program:`apt`. To do that, run the following commands as root or with sudo:

   .. code-block:: bash

     $ sudo apt-key adv --keyserver keys.gnupg.net --recv-keys 1C4CBDCDCD2EFD2A

   .. note::

     In case you're getting timeouts when using ``keys.gnupg.net`` as an alternative you can fetch the key from ``keyserver.ubuntu.com``.

2. Create the :program:`apt` source list for Percona's repository:

   You can create the source list and add the Percona repository by running:

   .. code-block:: bash

     $ echo "deb http://repo.percona.com/apt "$(lsb_release -sc)" main" | sudo tee /etc/apt/sources.list.d/percona.list

   Additionally you can enable the source package repository by running:

   .. code-block:: bash

     $ echo "deb-src http://repo.percona.com/apt "$(lsb_release -sc)" main" | sudo tee -a /etc/apt/sources.list.d/percona.list

3. Remember to update the local cache:

   .. code-block:: bash

     $ sudo apt-get update

4. After that you can install the package:

   .. code-block:: bash

     $ sudo apt-get install percona-xtrabackup


.. _debian_testing:

Percona ``apt`` Testing repository
----------------------------------

Percona offers pre-release builds from the testing repository. To enable it add the just add the ``testing`` word at the end of the Percona repository definition in your repository file (default :file:`/etc/apt/sources.list.d/percona.list`). It should looks like this (in this example ``VERSION`` is the name of your distribution): ::

  deb http://repo.percona.com/apt VERSION main testing
  deb-src http://repo.percona.com/apt VERSION main testing

Apt-Pinning the packages
------------------------

In some cases you might need to "pin" the selected packages to avoid the upgrades from the distribution repositories. You'll need to make a new file :file:`/etc/apt/preferences.d/00percona.pref` and add the following lines in it: ::

  Package: *
  Pin: release o=Percona Development Team
  Pin-Priority: 1001

For more information about the pinning you can check the official `debian wiki <http://wiki.debian.org/AptPreferences>`_.

.. _standalone_deb:

Installing |Percona XtraBackup| using downloaded deb packages
=============================================================

Download the packages of the desired series for your architecture from the `download page <https://www.percona.com/downloads/XtraBackup/>`_. Following example will download |Percona XtraBackup| 2.3.2 release package for *Debian* 8.0:

.. code-block:: bash

  $ wget https://www.percona.com/downloads/XtraBackup/Percona-XtraBackup-2.3.2/binary/debian/jessie/x86_64/percona-xtrabackup_2.3.2-1.jessie_amd64.deb

Now you can install |Percona XtraBackup| by running:

.. code-block:: bash

  $ sudo dpkg -i percona-xtrabackup_2.3.2-1.jessie_amd64.deb

.. note::

  When installing packages manually like this, you'll need to make sure to resolve all the dependencies and install missing packages yourself.

Uninstalling |Percona XtraBackup|
=================================

To uninstall |Percona XtraBackup| you'll need to remove all the installed packages.

2. Remove the packages

   .. code-block:: bash

     $ sudo apt-get remove percona-xtrabackup
