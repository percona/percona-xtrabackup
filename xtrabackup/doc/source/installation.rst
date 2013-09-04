===============================================
 Installing |Percona XtraBackup| from Binaries
===============================================

Before installing, you might want to read the :doc:`release-notes`.

Ready-to-use binaries are available from the |Percona XtraBackup| `download page <http://www.percona.com/downloads/XtraBackup/>`_, including:

 * ``RPM`` packages for *RHEL* 5 and *RHEL* 6 (including compatible distributions such as CentOS and Oracle Enterprise Linux)

 * *Debian* packages for *Debian* and *Ubuntu*

 * Generic ``.tar.gz`` binary packages

Using Percona Software Repositories
===================================

.. toctree::
   :maxdepth: 1

   installation/apt_repo
   installation/yum_repo

|Percona| provides repositories for :program:`yum` (``RPM`` packages for *Red Hat Enterprise Linux* and compatible distributions such as *CentOS*, *Oracle Enterprise Linux*, *Amazon Linux AMI*, and *Fedora*) and :program:`apt` (:file:`.deb` packages for *Ubuntu* and *Debian*) for software such as |Percona Server|, |XtraDB|, |Percona XtraBackup|, and |Percona Toolkit|. This makes it easy to install and update your software and its dependencies through your operating system's package manager.

This is the recommend way of installing where possible.
