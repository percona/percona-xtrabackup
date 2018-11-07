.. _apt_repo:

==========================================================
 Installing |Percona XtraBackup| on *Debian* and *Ubuntu*
==========================================================

Ready-to-use packages are available from the |Percona XtraBackup| software
repositories and the `download page
<https://www.percona.com/downloads/XtraBackup/>`_.

Supported releases:

* Debian 8.0 (jessie)
* Debian 9.0 (stretch)
* Ubuntu 16.04 (xenial)
* Ubuntu 18.04 LTS (bionic)

Supported architectures:

* x86_64 (also known as ``amd64``)

What's in each DEB package?
================================================================================

The ``percona-xtrabackup-80`` package contains the latest |Percona XtraBackup|
GA binaries and associated files.

The ``percona-xtrabackup-dbg-80`` package contains the debug symbols for
binaries in ``percona-xtrabackup-80``.

The ``percona-xtrabackup-test-80`` package contains the test suite for
|Percona XtraBackup|.

The ``percona-xtrabackup`` package contains the older version of the
|Percona XtraBackup|.

Installing |Percona XtraBackup| from Percona ``apt`` repository
===============================================================

1. Fetch the repository packages from Percona web:

   .. code-block:: bash

      $ wget https://repo.percona.com/apt/percona-release_latest.$(lsb_release -sc)_all.deb

#. Install the downloaded package with :program:`dpkg`. To do that, run the
   following commands as root or with :program:`sudo`: :bash:`dpkg -i percona-release_latest.$(lsb_release -sc)_all.deb`

   Once you install this package the Percona repositories should be added. You
   can check the repository setup in the
   :file:`/etc/apt/sources.list.d/percona-release.list` file.

.. include:: ../.res/contents/instruction.repository.enabling.txt

#. Remember to update the local cache: :bash:`apt-get update`

#. After that you can install the package: :bash:`apt-get install percona-xtrabackup-80`

Apt-Pinning the packages
========================

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
`download page <https://www.percona.com/downloads/XtraBackup/>`_. The following
example will download |Percona XtraBackup| 8.0.4-1 release package for *Debian*
8.0:

.. code-block:: bash
         
  $ wget https://www.percona.com/downloads/XtraBackup/Percona-XtraBackup-8.0.4/binary/debian/stretch/x86_64/percona-xtrabackup-80_8.0.4-1.stretch_amd64.deb

Now you can install |Percona XtraBackup| by running:

.. code-block:: bash

  $ sudo dpkg -i percona-xtrabackup-80_0.4-1.stretch_amd64.deb

.. note::

   When installing packages manually like this, you'll need to make sure to
   resolve all the dependencies and install missing packages yourself.

Uninstalling |Percona XtraBackup|
=================================

To uninstall |Percona XtraBackup| you'll need to remove all the installed
packages.

#. Remove the packages

   .. code-block:: bash

      $ sudo apt-get remove percona-xtrabackup-80
