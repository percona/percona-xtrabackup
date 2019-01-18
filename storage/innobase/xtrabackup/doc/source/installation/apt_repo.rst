.. _apt_repo:

========================================================
Installing |Percona XtraBackup| on *Debian* and *Ubuntu*
========================================================

Ready-to-use packages are available from the |Percona XtraBackup| software
repositories and the `download page
<https://www.percona.com/downloads/XtraBackup/>`_.

Supported Releases:

* Debian:

 * 7.0 (wheezy)
 * 8.0 (jessie)
 * 9.0 (stretch)

* Ubuntu:

 * 14.04LTS (trusty)
 * 16.04LTS (xenial)
 * 17.04 (zesty)
 * 17.10 (artful)
 * 18.04 (bionic)

Supported Platforms:

* x86
* x86_64 (also known as ``amd64``)

What's in each DEB package?
===========================

The ``percona-xtrabackup-24`` package contains the latest |Percona XtraBackup|
GA binaries and associated files.

The ``percona-xtrabackup-dbg-24`` package contains the debug symbols for
binaries in ``percona-xtrabackup-24``.

The ``percona-xtrabackup-test-24`` package contains the test suite for
|Percona XtraBackup|.

The ``percona-xtrabackup`` package contains the older version of the
|Percona XtraBackup|.

Installing |Percona XtraBackup| from Percona ``apt`` repository
===============================================================

1. Fetch the repository packages from Percona web:

   .. code-block:: bash
            
     $ wget https://repo.percona.com/apt/percona-release_latest.$(lsb_release -sc)_all.deb

2. Install the downloaded package with :program:`dpkg`. To do that, run the
   following commands as root or with :program:`sudo`:

   .. code-block:: bash

     $ sudo dpkg -i percona-release_latest.$(lsb_release -sc)_all.deb

   Once you install this package the Percona repositories should be added. You
   can check the repository setup in the
   :file:`/etc/apt/sources.list.d/percona-release.list` file.

3. Remember to update the local cache:

   .. code-block:: bash

     $ sudo apt-get update

4. After that you can install the package:

   .. code-block:: bash

     $ sudo apt-get install percona-xtrabackup-24

.. _debian_testing:

Percona ``apt`` Testing repository
----------------------------------

Percona offers pre-release builds from the testing repository. To enable it add
the just add the ``testing`` word at the end of the Percona repository
definition in your repository file (default
:file:`/etc/apt/sources.list.d/percona-release.list`). It should looks like
this (in this example ``VERSION`` is the name of your distribution):

.. code-block:: text

  deb http://repo.percona.com/apt VERSION main testing
  deb-src http://repo.percona.com/apt VERSION main testing

For example, if you are running *Debian* 8 (*jessie*) and want to install the
latest testing builds, the definitions should look like this:

.. code-block:: text

  deb http://repo.percona.com/apt jessie main testing
  deb-src http://repo.percona.com/apt jessie main testing

Apt-Pinning the packages
------------------------

In some cases you might need to "pin" the selected packages to avoid the
upgrades from the distribution repositories. You'll need to make a new file
:file:`/etc/apt/preferences.d/00percona.pref` and add the following lines in
it:

.. code-block:: text

  Package: *
  Pin: release o=Percona Development Team
  Pin-Priority: 1001

For more information about the pinning you can check the official
`debian wiki <http://wiki.debian.org/AptPreferences>`_.

.. _standalone_deb:

Installing |Percona XtraBackup| using downloaded deb packages
=============================================================

Download the packages of the desired series for your architecture from the
`download page <https://www.percona.com/downloads/XtraBackup/>`_. Following
example will download |Percona XtraBackup| 2.4.4 release package for *Debian*
8.0:

.. code-block:: bash

  $ wget https://www.percona.com/downloads/XtraBackup/Percona-XtraBackup-2.4.4/\
  binary/debian/jessie/x86_64/percona-xtrabackup-24_2.4.4-1.jessie_amd64.deb

Now you can install |Percona XtraBackup| by running:

.. code-block:: bash

  $ sudo dpkg -i percona-xtrabackup-24_2.4.4-1.jessie_amd64.deb

.. note::

  When installing packages manually like this, you'll need to make sure to
  resolve all the dependencies and install missing packages yourself.

Uninstalling |Percona XtraBackup|
=================================

To uninstall |Percona XtraBackup| you'll need to remove all the installed
packages.

2. Remove the packages

   .. code-block:: bash

     $ sudo apt-get remove percona-xtrabackup-24
