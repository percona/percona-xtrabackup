.. _yum_repo:

======================================================================
Installing |Percona XtraBackup| on Red Hat Enterprise Linux and CentOS
======================================================================

Ready-to-use packages are available from the |Percona XtraBackup| software
repositories and the `download page
<https://www.percona.com/downloads/XtraBackup/>`_. The |Percona|
:program:`yum` repository supports popular *RPM*-based operating systems,
including the *Amazon Linux AMI*.

The easiest way to install the *Percona Yum* repository is to install an *RPM*
that configures :program:`yum` and installs the `Percona GPG key
<https://www.percona.com/downloads/RPM-GPG-KEY-percona>`_.

Supported Releases:


 * *CentOS* 5 and *RHEL* 5

 * *CentOS* 6 and *RHEL* 6 (Current Stable) [#f1]_

 * *CentOS* 7 and *RHEL* 7

 * *Amazon Linux AMI* (works the same as *CentOS* 6)

The *CentOS* repositories should work well with *Red Hat Enterprise Linux* too,
provided that :program:`yum` is installed on the server.

Supported Platforms:

 * x86
 * x86_64 (also known as ``amd64``)

What's in each RPM package?
===========================

The ``percona-xtrabackup-24`` package contains the latest |Percona XtraBackup|
GA binaries and associated files.

The ``percona-xtrabackup-24-debuginfo`` package contains the debug symbols for
binaries in ``percona-xtrabackup-24``.

The ``percona-xtrabackup-test-24`` package contains the test suite for |Percona
XtraBackup|.

The ``percona-xtrabackup`` package contains the older version of the
|Percona XtraBackup|.

Installing |Percona XtraBackup| from Percona ``yum`` repository
===============================================================

1. Install the Percona repository

   You can install Percona yum repository by running the following command as a
   ``root`` user or with :program:`sudo`:

   .. code-block:: bash

      $ yum install https://repo.percona.com/yum/percona-release-latest.noarch.rpm

   You should see some output such as the following:

   .. code-block:: bash

     Retrieving https://repo.percona.com/yum/percona-release-latest.noarch.rpm
     Preparing...                ########################################### [100%]
        1:percona-release        ########################################### [100%]

.. note::

  *RHEL*/*Centos* 5 doesn't support installing the packages directly from the
  remote location so you'll need to download the package first and install it
  manually with :program:`rpm`:

  .. code-block:: bash

    $ wget https://repo.percona.com/yum/percona-release-latest.noarch.rpm
    $ rpm -ivH percona-release-latest.noarch.rpm

2. Testing the repository

   Make sure packages are now available from the repository, by executing the
   following command:

   .. code-block:: bash

     yum list | grep percona

   You should see output similar to the following:

   .. code-block:: bash

     ...
     percona-xtrabackup-20.x86_64               2.0.8-587.rhel5             percona-release-x86_64
     percona-xtrabackup-20-debuginfo.x86_64     2.0.8-587.rhel5             percona-release-x86_64
     percona-xtrabackup-20-test.x86_64          2.0.8-587.rhel5             percona-release-x86_64
     percona-xtrabackup-21.x86_64               2.1.9-746.rhel5             percona-release-x86_64
     percona-xtrabackup-21-debuginfo.x86_64     2.1.9-746.rhel5             percona-release-x86_64
     percona-xtrabackup-22.x86_64               2.2.13-1.el5                percona-release-x86_64
     percona-xtrabackup-22-debuginfo.x86_64     2.2.13-1.el5                percona-release-x86_64
     percona-xtrabackup-debuginfo.x86_64        2.3.5-1.el5                 percona-release-x86_64
     percona-xtrabackup-test.x86_64             2.3.5-1.el5                 percona-release-x86_64
     percona-xtrabackup-test-21.x86_64          2.1.9-746.rhel5             percona-release-x86_64
     percona-xtrabackup-test-22.x86_64          2.2.13-1.el5                percona-release-x86_64
     ...

3. Install the packages

   You can now install |Percona XtraBackup| by running:

   .. code-block:: bash

     yum install percona-xtrabackup-24

.. warning::

   In order to sucessfully install |Percona XtraBackup| on CentOS prior to version 7, the ``libev`` package
   needs to be installed first. This package ``libev`` package can be installed from the
   `EPEL <https://fedoraproject.org/wiki/EPEL>`_ repositories.

.. _yum_testing:

Percona `yum` Testing Repository
================================

Percona offers pre-release builds from our testing repository. To subscribe to
the testing repository, you'll need to enable the testing repository in
:file:`/etc/yum.repos.d/percona-release.repo`. To do so, set both
``percona-testing-$basearch`` and ``percona-testing-noarch`` to
``enabled = 1`` (Note that there are 3 sections in this file: release, testing
and experimental - in this case it is the second section that requires
updating). **NOTE:** You'll need to install the Percona repository first (ref
above) if this hasn't been done already.

.. _standalone_rpm:

Installing |Percona XtraBackup| using downloaded rpm packages
=============================================================

Download the packages of the desired series for your architecture from the
`download page <https://www.percona.com/downloads/XtraBackup/>`_. Following
example will download |Percona XtraBackup| 2.4.4 release package for
*CentOS* 7:

.. code-block:: bash

  $ wget https://www.percona.com/downloads/XtraBackup/Percona-XtraBackup-2.4.4/\
  binary/redhat/7/x86_64/percona-xtrabackup-24-2.4.4-1.el7.x86_64.rpm

Now you can install |Percona XtraBackup| by running:

.. code-block:: bash

 $ yum localinstall percona-xtrabackup-24-2.4.4-1.el7.x86_64.rpm

.. note::

  When installing packages manually like this, you'll need to make sure to
  resolve all the dependencies and install missing packages yourself.

Uninstalling |Percona XtraBackup|
=================================

To completely uninstall |Percona XtraBackup| you'll need to remove all the
installed packages.

Remove the packages

.. code-block:: bash

  yum remove percona-xtrabackup

.. rubric:: Footnotes

.. [#f1]

  "Current Stable": We support only the current stable RHEL6/CentOS6
  release, because there is no official (i.e. RedHat provided) method to
  support or download the latest OpenSSL on RHEL/CentOS versions prior to 6.5.
  Similarly, and also as a result thereof, there is no official Percona way to
  support the latest Percona XtraBackup builds on RHEL/CentOS versions prior to
  6.5. Additionally, many users will need to upgrade to OpenSSL 1.0.1g or later
  (due to the `Heartbleed vulnerability
  <http://www.percona.com/resources/ceo-customer-advisory-heartbleed>`_), and
  this OpenSSL version is not available for download from any official
  RHEL/CentOS repository for versions 6.4 and prior. For any officially
  unsupported system, src.rpm packages may be used to rebuild |Percona
  XtraBackup| for any environment. Please contact our `support service
  <http://www.percona.com/products/mysql-support>`_ if you require further
  information on this.
