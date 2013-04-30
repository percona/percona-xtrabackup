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

Ubuntu
------

 * 10.04LTS (lucid)
 * 12.04LTS (precise) 
 * 12.10 (quantal)

Experimental Repository
=======================

Percona now offers experimental repository for beta releases. To enable it add this to :file:`/etc/apt/sources.list`, replacing ``VERSION`` with the name of your distribution: ::

  deb http://repo.percona.com/apt VERSION main experimental
  deb-src http://repo.percona.com/apt VERSION main experimental

