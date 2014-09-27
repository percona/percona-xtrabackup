===================================
 Percona :program:`apt` Repository
===================================

*Debian* and *Ubuntu* packages from *Percona* are signed with a key. Before using the repository, you should add the key to :program:`apt`. To do that, run the following commands: ::

  $ apt-key adv --keyserver keys.gnupg.net --recv-keys 1C4CBDCDCD2EFD2A

Add this to :file:`/etc/apt/sources.list`, replacing ``VERSION`` with the name of your distribution: ::

  deb http://repo.percona.com/apt VERSION main
  deb-src http://repo.percona.com/apt VERSION main

Remember to update the local cache: ::

  $ apt-get update

Supported Architectures
=======================

 * x86_64 (also known as amd64)
 * x86

Supported Releases
==================

Debian
------

 * 6.0 (squeeze)
 * 7.0 (wheezy)

Ubuntu
------

 * 10.04LTS (lucid)
 * 12.04LTS (precise) 
 * 12.10 (quantal)
 * 13.04 (raring)
 * 13.10 (saucy)

Percona `apt` Testing repository
=================================

Percona offers pre-release builds from the testing repository. To enable it add the following lines to your  :file:`/etc/apt/sources.list` , replacing ``VERSION`` with the name of your distribution: ::

  deb http://repo.percona.com/apt VERSION main testing
  deb-src http://repo.percona.com/apt VERSION main testing

Ubuntu PPA of daily builds
==========================

Percona offers a Personal Package Archive (PPA) `percona-daily/percona-xtrabackup <https://launchpad.net/~percona-daily/+archive/percona-xtrabackup>`_. Every time code is pushed to our main source code repository, the PPA gets updated.

.. note:: 

  These packages are directly built from lp:percona-xtrabackup/2.1 trunk and should be considered PRE-RELEASE SOFTWARE.

This PPA can be added to your system manually by copying the lines below and adding them to your system's software sources: :: 
  
  deb http://ppa.launchpad.net/percona-daily/percona-xtrabackup/ubuntu YOUR_UBUNTU_VERSION_HERE main 
  deb-src http://ppa.launchpad.net/percona-daily/percona-xtrabackup/ubuntu YOUR_UBUNTU_VERSION_HERE main

or by simply running: :: 

  $ sudo add-apt-repository ppa:percona-daily/percona-xtrabackup

