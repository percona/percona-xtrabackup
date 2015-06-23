.. _apt_repo:

===================================
 Percona :program:`apt` Repository
===================================

*Debian* and *Ubuntu* packages from *Percona* are signed with a key. Before using the repository, you should add the key to :program:`apt`. To do that, run the following commands: ::

  $ apt-key adv --keyserver keys.gnupg.net --recv-keys 1C4CBDCDCD2EFD2A

Add this to :file:`/etc/apt/sources.list`, replacing ``VERSION`` with the name of your distribution: ::

  deb http://repo.percona.com/apt VERSION main testing
  deb-src http://repo.percona.com/apt VERSION main testing

Remember to update the local cache: ::

  $ apt-get update

Now you can install |Percona XtraBackup| 2.3 with: ::

  $ apt-get install percona-xtrabackup-2.3

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
 * 8.0 (jessie)

Ubuntu
------

 * 12.04LTS (precise) 
 * 14.04LTS (trusty)
 * 14.10 (utopic)
 * 15.04 (vivid)

.. _debian_testing: 

Percona `apt` Testing repository
=================================

Percona offers pre-release builds from the testing repository. To enable it add the following lines to your  :file:`/etc/apt/sources.list` , replacing ``VERSION`` with the name of your distribution: ::

  deb http://repo.percona.com/apt VERSION main testing
  deb-src http://repo.percona.com/apt VERSION main testing
