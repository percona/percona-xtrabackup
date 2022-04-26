.. _apt_repo:

==========================================================
 Installing *Percona XtraBackup* on *Debian* and *Ubuntu*
==========================================================

Ready-to-use packages are available from the *Percona XtraBackup* software
repositories and the `download page
<https://www.percona.com/downloads/XtraBackup/>`_.

Specific information on the supported platforms, products, and versions is described in `Percona Release Lifecycle Overview <https://www.percona.com/services/policies/percona-software-platform-lifecycle#mysql>`_.

What's in each DEB package?
================================================================================

The ``percona-xtrabackup-80`` package contains the latest *Percona XtraBackup*
GA binaries and associated files.

The ``percona-xtrabackup-dbg-80`` package contains the debug symbols for
binaries in ``percona-xtrabackup-80``.

The ``percona-xtrabackup-test-80`` package contains the test suite for
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

#. Install the downloaded package with :program:`dpkg`. To do that, run the
   following commands as root or with :program:`sudo`: :bash:`dpkg -i percona-release_latest.$(lsb_release -sc)_all.deb`

   Once you install this package the Percona repositories should be added. You
   can check the repository setup in the
   :file:`/etc/apt/sources.list.d/percona-release.list` file.

#. Enable the repository: :bash:`percona-release enable-only tools release`

   If *Percona XtraBackup* is intended to be used in combination with
   the upstream MySQL Server, you only need to enable the ``tools``
   repository: :bash:`percona-release enable-only tools`.

#. Remember to update the local cache: :bash:`apt update`

#. After that you can install the ``percona-xtrabackup-80`` package:

   .. code-block:: bash

      $ sudo apt install percona-xtrabackup-80

#. In order to make compressed backups, install the ``qpress`` package:

   .. code-block:: bash

      $ sudo apt install qpress

   .. seealso:: :ref:`compressed_backup`

   .. note:: 

      For AppArmor profile information, see :ref:`pxb-apparmor`.

Apt-Pinning the packages
========================

In some cases you might need to "pin" the selected packages to avoid the
upgrades from the distribution repositories. Make a new file
:file:`/etc/apt/preferences.d/00percona.pref` and add the following lines in
it:

.. code-block:: text

   Package: *
   Pin: release o=Percona Development Team
   Pin-Priority: 1001

For more information about the pinning, check the official
`debian wiki <http://wiki.debian.org/AptPreferences>`_.

.. _standalone_deb:

Installing *Percona XtraBackup* using downloaded deb packages
=============================================================

Download the packages of the desired series for your architecture from `Download Percona XtraBackup 8.0 <https://www.percona.com/downloads/XtraBackup/>`_. The following
example downloads *Percona XtraBackup* 8.0.26-18 release package for Ubuntu 20.04:

.. code-block:: bash

  $ wget https://downloads.percona.com/downloads/Percona-XtraBackup-LATEST/Percona-XtraBackup-8.0.26-18/binary/debian/focal/x86_64/percona-xtrabackup-80_8.0.26-18-1.focal_amd64.deb

Install *Percona XtraBackup* by running:

.. code-block:: bash

  $ sudo dpkg -i percona-xtrabackup-80_8.0.26-18-1.focal_amd64.deb

.. note::

   When installing packages manually like this, resolve all the dependencies and install missing packages yourself.

Uninstalling *Percona XtraBackup*
=================================

To uninstall *Percona XtraBackup*, remove all the installed
packages.

#. Remove the packages

   .. code-block:: bash

      $ sudo apt remove percona-xtrabackup-80

.. |percona-release| replace:: ``percona-release``
