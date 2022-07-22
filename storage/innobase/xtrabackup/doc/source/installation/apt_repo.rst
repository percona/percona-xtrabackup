.. _apt_repo:

========================================================
Installing *Percona XtraBackup* on *Debian* and *Ubuntu*
========================================================

Ready-to-use packages are available from the *Percona XtraBackup* software
repositories and the `Percona download page
<https://www.percona.com/downloads/XtraBackup/>`_.

Specific information on the supported platforms, products, and versions is described in `Percona Software and Platform Lifecycle <https://www.percona.com/services/policies/percona-software-platform-lifecycle#mysql>`_.

What's in each DEB package?
===========================

The ``percona-xtrabackup-24`` package contains the latest *Percona XtraBackup*
GA binaries and associated files.

The ``percona-xtrabackup-dbg-24`` package contains the debug symbols for
binaries in ``percona-xtrabackup-24``.

The ``percona-xtrabackup-test-24`` package contains the test suite for
*Percona XtraBackup*.

The ``percona-xtrabackup`` package contains the older version of the
*Percona XtraBackup*.

Installing *Percona XtraBackup* via *percona-release*
================================================================================

*Percona XtraBackup*, like many other *Percona* products, is installed
with the *percona-release* package configuration tool.

1. Download a deb package for *percona-release* the repository packages from Percona web:

   .. code-block:: bash
            
     $ wget https://repo.percona.com/apt/percona-release_latest.$(lsb_release -sc)_all.deb

2. Install the downloaded package with `dpkg`. To do that, run the
   following commands as root or with `sudo`:

   .. code-block:: bash

     $ sudo dpkg -i percona-release_latest.$(lsb_release -sc)_all.deb

   Once you install this package the Percona repositories should be added. You
   can check the repository setup in the
   `/etc/apt/sources.list.d/percona-release.list` file.

#. Enable the repository: :bash:`percona-release enable-only tools release`

   If *Percona XtraBackup* is intended to be used in combination with
   the upstream MySQL Server, you enable only the ``tools``
   repository: :bash:`percona-release enable-only tools`.

#. After that you can install the ``percona-xtrabackup-24`` package:

   .. code-block:: bash
		   
      $ sudo apt install percona-xtrabackup-24

#. In order to make compressed backups, install the ``qpress`` package:

   .. code-block:: bash

      $ sudo apt install qpress

   .. seealso:: :ref:`compressed_backup`

Apt-Pinning the packages
------------------------

In some cases you might need to "pin" the selected packages to avoid the
upgrades from the distribution repositories. You'll need to make a new file
`/etc/apt/preferences.d/00percona.pref` and add the following lines in
it:

.. code-block:: text

  Package: *
  Pin: release o=Percona Development Team
  Pin-Priority: 1001

For more information about the pinning you can check the official
`debian wiki <http://wiki.debian.org/AptPreferences>`_.

.. _standalone_deb:

Installing *Percona XtraBackup* using downloaded deb packages
=============================================================

Download the packages of the desired series for your architecture from the
`download page <https://www.percona.com/downloads/XtraBackup/>`_. Following
example downloads the *Percona XtraBackup* 2.4.20 release package for *Debian*
9.0:

.. code-block:: bash

  $ wget https://www.percona.com/downloads/XtraBackup/Percona-XtraBackup-2.4.20/\
  binary/debian/stretch/x86_64/percona-xtrabackup-24_2.4.20-1.stretch_amd64.deb

Now you can install *Percona XtraBackup* by running:

.. code-block:: bash

  $ sudo dpkg -i percona-xtrabackup-24_2.4.20-1.stretch_amd64.deb

.. note::

  Installing the packages manually like this, you must
  resolve all dependencies and install the missing packages yourself.

Update the Curl utility in Debian 10
=============================================

The default curl version, 7.64.0, in Debian 10 has known issues when attempting to reuse an already closed connection. This issue directly affects ``xbcloud`` and users may see intermittent backup failures. 

For more details, see `curl #3750 <https://github.com/curl/curl/issues/3750>`__ or `curl #3763 <https://github.com/curl/curl/pull/3763>`__. 

Follow these steps to upgrade curl to version 7.74.0: 


#. Edit the ``/etc/apt/sources.list`` to add the following:

   .. code-block:: text

      deb http://ftp.de.debian.org/debian buster-backports main

#. Refresh the ``apt`` sources:

   .. code-block:: bash

      sudo apt update

#. Install the version from ``buster-backports``:

   .. code-block:: bash

      $ sudo apt install curl/buster-backports

#. Verify the version number:

   .. code-block:: bash

      $ curl --version
      curl 7.74.0 (x86_64-pc-linux-gnu) libcurl/7.74.0 

Uninstalling *Percona XtraBackup*
==================================

To uninstall *Percona XtraBackup* you'll need to remove all the installed
packages.

2. Remove the packages

   .. code-block:: bash

      $ sudo apt remove percona-xtrabackup-24


