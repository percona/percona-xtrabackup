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

 * Amazon Linux 2

The *CentOS* repositories should work well with *Red Hat Enterprise Linux* too,
provided that :program:`yum` is installed on the server.

Supported Platforms:

 * x86
 * x86_64 (also known as ``amd64``)

What's in each RPM package?
===========================

The ``percona-xtrabackup-80`` package contains the latest |Percona XtraBackup|
GA binaries and associated files.

The ``percona-xtrabackup-80-debuginfo`` package contains the debug symbols for
binaries in ``percona-xtrabackup``.

The ``percona-xtrabackup-test-80`` package contains the test suite for |Percona
XtraBackup|.

The ``percona-xtrabackup`` package contains the older version of the
|Percona XtraBackup|.

Installing |Percona XtraBackup| from Percona ``yum`` repository
===============================================================

1. Install the Percona repository

   You can install Percona yum repository by running the following command as a
   ``root`` user or with :program:`sudo`:

   .. code-block:: bash

     $ sudo yum install https://repo.percona.com/centos/7/RPMS/noarch/percona-release-0.1-8.noarch.rpm

   You should see some output such as the following:

   .. code-block:: bash

     Retrieving http://www.percona.com/downloads/percona-release/redhat/0.1-6/percona-release-0.1-8.noarch.rpm
     Preparing...                ########################################### [100%]
        1:percona-release        ########################################### [100%]

.. note::

  *RHEL*/*Centos* 5 doesn't support installing the packages directly from the
  remote location so you'll need to download the package first and install it
  manually with :program:`rpm`:

  .. code-block:: bash

    $ wget http://www.percona.com/downloads/percona-release/redhat/0.1-6/\
    percona-release-0.1-6.noarch.rpm
    $ rpm -ivH percona-release-0.1-6.noarch.rpm

#. Enable the repository:

   .. code-block:: bash

      $ sudo percona-release enable tools testing

#. Install the packages

   You can now install |Percona XtraBackup| by running:

   .. code-block:: bash

     yum install percona-xtrabackup-80

.. warning::

   In order to sucessfully install |Percona XtraBackup| ``libev`` package will
   need to be installed first. ``libev`` package can be installed from the
   `EPEL <https://fedoraproject.org/wiki/EPEL>`_ repositories.

.. _standalone_rpm:

Installing |Percona XtraBackup| using downloaded rpm packages
=============================================================

Download the packages of the desired series for your architecture from the
`download page <https://www.percona.com/downloads/XtraBackup/>`_. Following
example will download |Percona XtraBackup| 8.0.12 release package for
*CentOS* 7:

.. code-block:: bash

  $ wget https://www.percona.com/downloads/XtraBackup/Percona-XtraBackup-8.0.12/\
  binary/redhat/7/x86_64/percona-xtrabackup-80-8.0.12-1.el7.x86_64.rpm

Now you can install |Percona XtraBackup| by running:

.. code-block:: bash

 $ yum localinstall percona-xtrabackup-80-8.0.12-1.el7.x86_64.rpm

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
