.. _version-check:

================================================================================
Version Checking
================================================================================

Some Percona software contains “version checking” functionality which is a
feature that enables Percona software users to be notified of available software
updates to improve your environment security and performance. Alongside this,
the version check functionality also provides Percona with information relating
to which software versions you are running, coupled with the environment
confirmation which the software is running within. This helps enable Percona to
focus our development effort accordingly based on trends within our customer
community.

The purpose of this document is to articulate the information that is collected,
as well as to provide guidance on how to disable this functionality if desired.

Usage
================================================================================

*Version Check* was implemented in |pt| 2.1.4, and was enabled by default in
version 2.2.1. Currently it is supported as a ``--[no]version-check`` option
by `a number of tools in Percona Toolkit <https://www.percona.com/doc/percona-toolkit/LATEST/genindex.html>`_,
|pxb|, and |pmm|.

When launched with Version Check enabled, the tool that supports this feature
connects to a Percona's *version check service* via a secure HTTPS channel. It
compares the locally installed version for possible updates, and also checks
versions of the following software:

* Operating System
* Percona Monitoring and Management (PMM)
* MySQL
* Perl
* MySQL driver for Perl (DBD::mysql)
* Percona Toolkit

Then it checks for and warns about versions with known problems if they are
identified as running in the environment.

Each version check request is logged by the server. Stored information consists
of the checked system unique ID followed by the software name and version.  The
ID is generated either at installation or when the |version-check| query is
submitted for the first time.

.. note::

   Prior to version 3.0.7 of |pt|, the system ID was calculated as an MD5 hash
   of a hostname, and starting from |pt| 3.0.7 it is generated as an MD5 hash of
   a random number. |pxb| continues to use hostname-based MD5 hash.

As a result, the content of the sent query is as follows::

  85624f3fb5d2af8816178ea1493ed41a;DBD::mysql;4.044
  c2b6d625ef3409164cbf8af4985c48d3;MySQL;MySQL Community Server (GPL) 5.7.22-log
  85624f3fb5d2af8816178ea1493ed41a;OS;Manjaro Linux
  85624f3fb5d2af8816178ea1493ed41a;Percona::Toolkit;3.0.11-dev
  85624f3fb5d2af8816178ea1493ed41a;Perl;5.26.2

Disabling Version Check
================================================================================

Although the |version-check| feature does not collect any personal information,
you might prefer to disable this feature, either one time or permanently.  To
disable it one time, use ``--no-version-check`` option when invoking the tool
from a Percona product which supports it. Here is a simple example which shows
running `pt-diskstats
<https://www.percona.com/doc/percona-toolkit/LATEST/pt-diskstats.html>`_ tool
from the |pt| with |version-check| turned off::

  pt-diskstats --no-version-check

Disabling |version-check| permanently can be done by placing
``no-version-check`` option into the configuration file of a Percona product
(see correspondent documentation for exact file name and syntax). For example,
in case of |pt| `this can be done
<https://www.percona.com/doc/percona-toolkit/LATEST/configuration_files.html>`_
in a global configuration file ``/etc/percona-toolkit/percona-toolkit.conf``::

  # Disable Version Check for all tools:
  no-version-check

In case of |pxb| this can be done `in its configuration file
<https://www.percona.com/doc/percona-xtrabackup/2.4/using_xtrabackup/configuring.htm>`_
in a similar way::

  [xtrabackup]
  no-version-check

Frequently Asked Questions
================================================================================

.. contents::
   :local:

Why is this functionality enabled by default?
--------------------------------------------------------------------------------

We believe having this functionality enabled improves security and performance
of environments running Percona Software and it is good choice for majority of
the users.

Why not rely on Operating System's built in functionality for software updates?
--------------------------------------------------------------------------------

In many environments the Operating Systems repositories may not carry the latest
version of software and newer versions of software often installed manually, so
not being covered by operating system wide check for updates.

Why do you send more information than just the version of software being run as a part of version check service?
-----------------------------------------------------------------------------------------------------------------------

Compatibility problems can be caused by versions of various components in the
environment, for example problematic versions of Perl, DBD or MySQL could cause
operational problems with Percona Toolkit.

.. |pmm| replace:: PMM (Percona Monitoring and Management)
.. |pt| replace:: Percona Toolkit
.. |pxb| replace:: Percona XtraBackup
.. |version-check| replace:: *version checking*
