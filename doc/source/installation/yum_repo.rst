===================================
 Percona :program:`yum` Repository
===================================

The |Percona| :program:`yum` repository supports popular *RPM*-based operating systems, including the *Amazon Linux AMI*.

The easiest way to install the *Percona Yum* repository is to install an *RPM* that configures :program:`yum` and installs the `Percona GPG key <http://www.percona.com/downloads/RPM-GPG-KEY-percona>`_. You can also do the installation manually.

Automatic Installation
======================

Execute the following command as a ``root`` user: ::

  $ yum install http://www.percona.com/downloads/percona-release/redhat/0.1-3/percona-release-0.1-3.noarch.rpm 

You should see some output such as the following: ::

  Retrieving yum install http://www.percona.com/downloads/percona-release/redhat/0.1-3/percona-release-0.1-3.noarch.rpm
  Preparing...                ########################################### [100%]
     1:percona-release        ########################################### [100%]


Testing The Repository
======================

Make sure packages are downloaded from the repository, by executing the following command as root: ::

  yum list | grep percona

You should see output similar to the following: ::

  percona-release.x86_64                     0.0-1                       installed
  ...
  Percona-Server-client-51.x86_64            5.1.47-rel11.1.51.rhel5     percona  
  Percona-Server-devel-51.x86_64             5.1.47-rel11.1.51.rhel5     percona  
  Percona-Server-server-51.x86_64            5.1.47-rel11.1.51.rhel5     percona  
  Percona-Server-shared-51.x86_64            5.1.47-rel11.1.51.rhel5     percona  
  Percona-Server-test-51.x86_64              5.1.47-rel11.1.51.rhel5     percona  
  ...
  xtrabackup.x86_64                          1.2-22.rhel5                percona  

Supported Platforms
===================
  
  *  ``x86_64``
  *  ``i386``

Supported Releases
==================

The *CentOS* repositories should work well with *Red Hat Enterprise Linux* too, provided that :program:`yum` is installed on the server.

* *CentOS* 5 and *RHEL* 5
* *CentOS* 6 and *RHEL* 6
* *Amazon Linux AMI* (works the same as *CentOS* 5)

Percona `yum` Testing Repository
=================================

Percona offers pre-release builds from the testing repository. To subscribe to the testing repository, you'll need to enable the testing repository in :file:`/etc/yum.repos.d/percona-release.repo`. **NOTE:** You'll need to install the Percona repository first if this hasn't been done already.
