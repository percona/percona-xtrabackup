======================================
 |Percona| |XtraBackup| Release Notes
======================================

|Percona| |XtraBackup| 1.6.3
============================

Percona is glad to announce the release of Percona XtraBackup 1.6.3 on 22 September, 2011 (Downloads are available `here <http://www.percona.com/downloads/XtraBackup/XtraBackup-1.6/>`_ and from the :doc:`Percona Software Repositories <installation>`).

This release is purely composed of bug fixes and is the current stable release of |Percona| |Xtrabackup|.

If the :term:`innodb_file_per_table` server option is been used and ``DDL`` operations, ``TRUNCATE TABLE``, ``DROP/CREATE the_same_table`` or ``ALTER`` statements on |InnoDB| tables are executed while taking a backup, an upgrade to |XtraBackup| 1.6.3 is **strongly recommended**. Under this scenario, if the server version is prior to 5.5.11 in 5.5 series or prior to 5.1.49 in 5.1 series, a server upgrade is also recommended.

All of |Percona| 's software is open-source and free, all the details of the release and its development process can be found in the `1.6.3 milestone at Launchpad <https://launchpad.net/percona-xtrabackup/+milestone/1.6.3>`_.


Bugs Fixed
----------

  * Streaming backups did not work for compressed |InnoDB| tables due to missing support for compressed pages in |tar4ibd|. Bug Fixed: :bug:`665210` (*Alexey Kopytov*).

  * |XtraBackup| failed when ``innodb_flush_method`` in the server configuration file was set to ``ALL_O_DIRECT``. Bug Fixed: :bug:`759225` (*Alexey Kopytov*).

  * Due to a regression introduced in |XtraBackup| 1.6.2, :command:`innobackupex --copy-back` did not work if the :command:`xtrabackup` binary was not specified explicitly with the :option:`--ibbackup` option. Bug Fixed: :bug:`817132` (*Alexey Kopytov*).

  * The :option:`--slave-info` option now works correctly with :option:`--safe-slave-backup` when either :option:`--no-lock` or :option:`--incremental` is also specified. Bug Fixed: :bug:`834657` (*Alexey Kopytov*).

  * :program:`tar4ibd` could fail with an error when processing doublewrite pages. Bug Fixed: :bug:`810269` (*Alexey Kopytov*).

  * Unsupported command line options could cause a :program:`tar4ibd` crash. Such options have been removed. Bug Fixed: :bug:`677279` (*Alexey Kopytov*).

  * Executing ``DDL`` operations, ``TRUNCATE TABLE``, ``DROP/CREATE the_same_table`` or ``ALTER`` statements on |InnoDB| tables while taking a backup could lead to a |xtrabackup| failure due to a tablespace ``ID`` mismatch when using per-table tablespaces. Note that this fix may not work correctly with |MySQL| 5.5 or |Percona Server| 5.5 prior to version 5.5.11. 5.1 releases from 5.1.49 or higher have been confirmed not to be affected. 
    If the :term:`innodb_file_per_table` option is been used, an upgrade to |XtraBackup| 1.6.3 is **strongly recommended**. Under this scenario, if the server version is prior to 5.5.11 in 5.5 series or prior to 5.1.49 in 5.1 series, a server upgrade is also recommended. Bug Fixed: :bug:`722638` (*Alexey Kopytov*).


Other Changes
-------------

  * Improvements and fixes on the |XtraBackup| Test Suite: :bug:`855035`, :bug:`787966` (*Alexey Kopytov*)

  * Improvements and fixes on distribution: :bug:`775463`, :bug:`745168`, :bug:`849872`, :bug:`785556` (*Ignacio Nin*)

  * Improvements and fixes on the |XtraBackup| Documentation: :bug:`837754`, :bug:`745185`, :bug:`836907` (*Rodrigo Gadea*)


|Percona| |XtraBackup| 1.6.2
============================

Percona is glad to announce the release of Percona XtraBackup 1.6.2 on 25 July, 2011 (Downloads are available `here <http://www.percona.com/downloads/XtraBackup/XtraBackup-1.6/>`_ and from the `Percona Software Repositories <http://www.percona.com/docs/wiki/repositories:start>`_).

This release is purely composed of bug fixes and is the current stable release of |Percona| |Xtrabackup|.

All of |Percona|'s software is open-source and free, all the details of the release and its development process can be found in the `1.6.2 milestone at Launchpad <https://launchpad.net/percona-xtrabackup/+milestone/1.6.2>`_.

New Options
-----------

:option:`--version`
~~~~~~~~~~~~~~~~~~~

   The :option:`--version` option has been added to the |xtrabackup| binary for printing its version. Previously, the version was displayed only while executing the binary without arguments or performing a backup. Bug Fixed: `#610614 <https://bugs.launchpad.net/bugs/610614>`_ (Alexey Kopytov).

Changes
-------

  * As exporting tables should only be used with :term:`innodb_file_per_table` set in the server, the variable is checked by |xtrabackup| when using the :option:`--export <innobackupex --export>` option. It will fail before applying the archived log without producing a potentially unusable backup. Bug Fixed: `#758888 <https://bugs.launchpad.net/bugs/758888>`_ (Alexey Kopytov).

Bugs Fixed
----------

  * When creating an :term:`InnoDB` with its own tablespace after taking a full backup, if the log files have been flushed, taking an incremental backup based on that full one would not contain the added table. This has been corrected by explicitly creating the tablespace before applying the delta files in such cases. Bug Fixed: `#766607 <https://bugs.launchpad.net/bugs/766607>`_ (Alexey Kopytov).

  * In some cases, |innobackupex| ignored the specified |xtrabackup| binary with the :option:`--ibbackup` option. Bug Fixed: `#729497 <https://bugs.launchpad.net/bugs/729497>`_ (Stewart Smith).

  * Minor file descriptors leaks in error cases were fixed. Bug Fixed: `#803718 <https://bugs.launchpad.net/bugs/803718>`_ (Stewart Smith).

Other Changes
-------------

   * Improvements and fixes on the XtraBackup Test Suite: `#744303 <https://bugs.launchpad.net/bugs/744303>`_, `#787966 < <https://bugs.launchpad.net/bugs/787966>`_ (Alexey Kopytov)

   * Improvements and fixes on platform-specific distribution: `#785556 <https://bugs.launchpad.net/bugs/785556>`_ (Ignacio Nin)

   * Improvements and fixes on the XtraBackup Documentation: `#745185 <https://bugs.launchpad.net/bugs/745185>`_, `#721339 <https://bugs.launchpad.net/bugs/721339>`_ (Rodrigo Gadea)

|Percona| |XtraBackup| 1.6
==========================

Released on April 12, 2011 (Downloads are available `here <http://www.percona.com/downloads/XtraBackup/XtraBackup-1.6/>`_ and from the `Percona Software Repositories <http://www.percona.com/docs/wiki/repositories:start>`_.)

Options Added
-------------

* Added option :option:`--extra-lsndir` to |innobackupex|. When specified for the backup phase, the option is passed to |xtrabackup|, and :term:`LSN` information is stored with the file in the specified directory. This is needed so that :term:`LSN` information is preserved during stream backup. (Vadim Tkachenko)

* Added option :option:`--incremental-lsn` to |innobackupex|. If specified, this option is passed directly to the |xtrabackup| binary and :program:`--incremental-basedir` is ignored. (Vadim Tkachenko)

* Added option :option:`--incremental-dir` to |innobackupex|. This option is passed directly to the |xtrabackup| binary. (Vadim Tkachenko)

* Added option :option:`--safe-slave-backup` to |innobackupex|. (Daniel Nichter)

* Added option :option:`--safe-slave-backup-timeout` to |innobackupex|. (Daniel Nichter)

Other Changes
-------------

* Eliminated some compiler warnings. (Stewart Smith)

* Ported |XtraBackup| to |MySQL| 5.1.55, |MySQL| 5.5.9, |Percona Server| 5.1.55-12.6, and |Percona Server| 5.5.9-20.1 code bases. The :command:`xtrabackup_55` binary is now based on |Percona Server| 5.5, rather than |MySQL| 5.5. Support for building against |InnoDB| plugin in |MySQL| 5.1 has been removed. (Alexey Kopytov)

* Updates were made to the built-in |innobackupex| usage docs. (Baron Schwartz, Fred Linhoss)

* Added a manual page for |XtraBackup|. (Aleksandr Kuzminsky)

* Disabled auto-creating :file:`ib_logfile*` when |innobackupex| is called with :option:`--redo-only` or with :option:`--incremental-dir`. If necessary :file:`ib_logfile*` can be created later with :command:`xtrabackup --prepare` call. (Vadim Tkachenko)

* Fixed |xtrabackup| exit code to improve portability: ``EXIT_SUCCESS`` on success and ``EXIT_FAILURE`` on a failure. (Aleksandr Kuzminsky)

* For portability, the |XtraBackup| build script now tries to link with ``libaio`` only on Linux. (Aleksandr Kuzminsky)

Bugs Fixed
----------

* `Bug #368945 <https://bugs.launchpad.net/bugs/368945>`_ - When option :option:`--prepare` was specified, an error message was requesting that ``datadir`` be set, even though it's not a required option. (Vadim Tkachenko)

* `Bug #420181 <https://bugs.launchpad.net/bugs/420181>`_ - The |innobackupex| script now backs up :term:`.CSV` tables. (Valentine Gostev)

* `Bug #597384 <https://bugs.launchpad.net/bugs/597384>`_ - The ``innobackup`` :option:`--include` option now handles non-|InnoDB| tables. (Vadim Tkachenko)

* `Bug #606981 <https://bugs.launchpad.net/bugs/606981>`_ - Streaming |InnoDB| files with |tar4ibd| could lead to filesystem hangs when |InnoDB| was configured to access data files with the ``O_DIRECT`` flag. The reason was that |tar4ibd| did not have support for ``O_DIRECT`` and simultaneous ``O_DIRECT`` + non-``O_DIRECT`` access to a file on Linux is disallowed. Fixed |innobackupex| and |tar4ibd| to use ``O_DIRECT`` on input |InnoDB| files if the value of ``innodb_flush_method`` is ``O_DIRECT`` in the |InnoDB| configuration. (Alexey Kopytov)

* `Bug #646647 <https://bugs.launchpad.net/bugs/646647>`_ - Removed the bogus warning about invalid data in the Perl version string in |innobackupex|. (Baron Schwartz)

* `Bug #672384 <https://bugs.launchpad.net/bugs/672384>`_ - When no log files can be found in the backup directory while executing :option:`xtrabackup --stats`, a descriptive error message is printed instead of crashing. (Alexey Kopytov)

* `Bug #688211 <https://bugs.launchpad.net/bugs/688211>`_ - Using the :option:`--password` option with |innobackupex| to specify MySQL passwords containing special shell characters (such as "&") did not work, even when the option value was properly quoted.

* `Bug #688417 <https://bugs.launchpad.net/bugs/688417>`_ - It's now possible to do incremental backups for compressed |InnoDB| tables.

* `Bug #701767 <https://bugs.launchpad.net/bugs/701767>`_ - The script ``innobackupex-1.5.1`` was renamed to |innobackupex|. Symbolic link ``innobackupex-1.5.1`` was created for backupward compatibility. (Vadim Tkachenko)

* `Bug #703070 <https://bugs.launchpad.net/bugs/703070>`_ - ``xtrabackup_55`` crashed with an assertion failure on non-Linux platforms. (Alexey Kopytov)

* `Bug #703077 <https://bugs.launchpad.net/bugs/703077>`_ - Building |xtrabackup| could fail on some platforms due to an incorrect argument to ``CMake``. Fixed by changing the ``-DWITH_ZLIB`` argument to lowercase, because that's what the ``CMake`` scripts actually expect. (Alexey Kopytov)

* `Bug #713799 <https://bugs.launchpad.net/bugs/713799>`_ - Dropping a table during a backup process could result in assertion failure in |xtrabackup|. Now it continues with a warning message about the dropped table. (Alexey Kopytov)

* `Bug #717784 <https://bugs.launchpad.net/bugs/717784>`_ - Performing parallel backups with the :option:`--parallel` option could cause |xtrabackup| to fail with the "cannot mkdir" error. (Alexey Kopytov)

Percona |XtraBackup| 1.5-Beta
=============================

Released December 13, 2010 (`downloads <http://www.percona.com/downloads/XtraBackup/XtraBackup-1.5/>`_)

This release adds additional functionality to Percona |XtraBackup| 1.4, the current general availability version of |XtraBackup|. This is a beta release.

Functionality Added or Changes
------------------------------

* Support for |MySQL| 5.5 databases has been implemented. (Yasufumi Kinoshita)

* |XtraBackup| can now be built from the |MySQL| 5.1.52, |MySQL| 5.5.7, or |Percona Server| 5.1.53-12 code bases (fixes bug `#683507 <https://bugs.launchpad.net/bugs/683507>`_). (Alexey Kopytov)

* The program is now distributed as three separate binaries:

  * |xtrabackup| - for use with |Percona Server| with the built-in |InnoDB| plugin

  * :command:`xtrabackup_51` - for use with MySQL 5.0 & 5.1 with built-in |InnoDB|

  * :command:`xtrabackup_55` - for use with |MySQL| 5.5 (this binary is not provided for the FreeBSD platform)

* Backing up only specific tables can now be done by specifying them in a file, using the :option:`--tables-file`. (Yasufumi Kinoshita & Daniel Nichter)

* Additional checks were added to monitor the rate the log file is being overwritten, to determine if |XtraBackup| is keeping up. If the log file is being overwritten faster than |XtraBackup| can keep up, a warning is given that the backup may be inconsistent. (Yasufumi Kinoyasu) 

* The |XtraBackup| binaries are now compiled with the ``-O3`` :command:`gcc` option, which may improve backup speed in stream mode in some cases.

* It is now possible to copy multiple data files concurrently in parallel threads when creating a backup, using the :option:`--parallel` option. See `The xtrabackup Option Reference <http://www.percona.com/docs/wiki/percona-xtrabackup:xtrabackup:option-and-variable-reference>`_ and `Parallel Backups <http://www.percona.com/docs/wiki/percona-xtrabackup:innobackupex:how_to_recipes#Parallel_Backups>`_. (Alexey Kopytov)

Bugs Fixed
----------

* `Bug #683507 <https://bugs.launchpad.net/bugs/683507>`_ - |xtrabackup| has been updated to build from the |MySQL| 5.1.52, |MySQL| 5.5.7, or |Percona Server| 5.1.53-12 code bases. (Alexey Kopytov)

Percona |XtraBackup| 1.4
========================

Released on November 22, 2010

Percona |XtraBackup| version 1.4 fixes problems related to incremental backups. If you do incremental backups, it's strongly recommended that you upgrade to this release.

Functionality Added or Changed
------------------------------

* `Incremental backups <http://www.percona.com/docs/wiki/percona-xtrabackup:xtrabackup:incremental>`_ have changed and now allow the restoration of full backups containing certain rollback transactions that previously caused problems. Please see `Preparing the Backups <http://www.percona.com/docs/wiki/percona-xtrabackup:xtrabackup:incremental#Preparing_the_Backups>`_  and the :option:`--apply-log-only`. (From |innobackupex|, the :option:`--redo-only` option should be used.) (Yasufumi Kinoshita)

  * The |XtraBackup| Test Suite was implemented and is now a standard part of each distribution. (Aleksandr Kuzminsky)

* Other New Features

  * The :option:`--prepare` now reports ``xtrabackup_binlog_pos_innodb`` if the information exists. (Yasufumi Kinoshita)

  * When :option:`--prepare` is used to restore a partial backup, the data dictionary is now cleaned and contains only tables that exist in the backup. (Yasufumi Kinoshita)

  * The :option:`--table` was extended to accept several regular expression arguments, separated by commas. (Yasufumi Kinoshita)

* Other Changes

  * Ported to the |Percona Server| 5.1.47 code base. (Yasufumi Kinoshita)

  * |XtraBackup| now uses the memory allocators of the host operating system, rather than the built-in |InnoDB| allocators (see `Using Operating System Memory Allocators <http://dev.mysql.com/doc/innodb-plugin/1.1/en/innodb-performance-use_sys_malloc.html>`_). (Yasufumi Kinoshita)

Bugs Fixed
----------

* `Bug #595770 <https://bugs.launchpad.net/bugs/595770>`_ - XtraBack binaries are now shipped containing debug symbols by default. (Aleksandr Kuzminsky)

* `Bug #589639 <https://bugs.launchpad.net/bugs/589639>`_ - Fixed a problem of hanging when tablespaces were deleted during the recovery process. (Yasufumi Kinoshita)

* `Bug #611960 <https://bugs.launchpad.net/bugs/611960>`_ - Fixed a segmentation fault in |xtrabackup|. (Yasufumi Kinoshita)

* Miscellaneous important fixes related to incremental backups. 

Version 1.3 (unreleased)
========================

Major changes:
--------------

* Port to |Percona Server| 5.1.47-11

* Separate into two binaries - xtrabackup for |Percona Server| and xtrabackup_50 for |MySQL| 5.x.

Fixed Bugs:
-----------

* Fixed `Bug #561106 <https://bugs.launchpad.net/percona-xtrabackup/+bug/561106>`_: incremental crash

* Fixed duplicate ``close()`` problem at ``xtrabackup_copy_datafile()``.
