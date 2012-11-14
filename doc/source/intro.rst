==========================
 About Percona Xtrabackup
==========================


*Percona XtraBackup* is the world's only open-source, free |MySQL| hot backup software that performs non-blocking backups for |InnoDB| and |XtraDB| databases. With *Percona XtraBackup*, you can achieve the following benefits:

* Backups that complete quickly and reliably

* Uninterrupted transaction processing during backups

* Savings on disk space and network bandwidth

* Automatic backup verification

* Higher uptime due to faster restore time

|XtraBackup| makes |MySQL| hot backups for all versions of |Percona Server|, |MySQL|, |MariaDB|, and |Drizzle|. It performs streaming, compressed, and incremental |MySQL| backups.

|Percona| |XtraBackup| works with |MySQL|, |MariaDB|, |Percona Server|, and |Drizzle| databases (support for |Drizzle| is beta). It supports completely non-blocking backups of |InnoDB|, |XtraDB|, and *HailDB* storage engines. In addition, it can back up the following storage engines by briefly pausing writes at the end of the backup: |MyISAM|, :term:`Merge <.MRG>`, and :term:`Archive <.ARM>`, including partitioned tables, triggers, and database options.

|Percona|'s enterprise-grade commercial `MySQL Support <http://www.percona.com/mysql-support/>`_ contracts include support for XtraBackup. We recommend support for critical production deployments.

MySQL Backup Tool Feature Comparison
====================================

.. raw:: html

   <table class="datatable" style="text-align: center;">
   <tbody style="text-align: center;"><tr><th class="label">Feature</th><th>Percona XtraBackup</th><th>MySQL Enterprise Backup<br>(InnoDB Hot Backup)</th></tr>
   <tr><td class="label">License</td><td style="text-align: center;">GPL</td><td style="text-align: center;">Proprietary</td></tr>
   <tr><td class="label">Price</td><td style="text-align: center;">Free</td><td style="text-align: center;"><a href="http://www.mysql.com/products/">$5000 per server</a></td></tr>
   <tr><td class="label">Open source</td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td></td></tr>
   <tr><td class="label">Non-blocking</td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td></tr>
   <tr><td class="label">InnoDB backups</td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td></tr>
   <tr><td class="label">MyISAM backups <sup><a href="#note-1">1</a></sup></td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td></tr>
   <tr><td class="label">Compressed backups</td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td></tr>
   <tr><td class="label">Partial backups</td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td></tr>
   <tr><td class="label">Throttling <sup><a href="#note-2">2</a></sup></td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td></tr>
   <tr><td class="label">Point-in-time recovery support</td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td></tr>
   <tr><td class="label">Incremental backups</td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td></tr>
   <tr><td class="label">Parallel backups</sup></td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td></td></tr>
   <tr><td class="label">Streaming backups</td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td></td></tr>
   <tr><td class="label">OS buffer optimizations <sup><a href="#note-3">3</a></sup></td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td></td></tr>
   <tr><td class="label">Export individual tables</td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td></td></tr>
   <tr><td class="label">Restore tables to a different server</td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td></td></tr>
   <tr><td class="label">Analyze data &amp; index files</td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td></td></tr>
   <tr><td class="label">Familiar command-line behavior <sup><a href="#note-4">4</a></sup></td><td style="text-align: center;"><img width="24" height="24" alt="Yes" src="http://s0.percona.com/check-yes.png"></td><td></td></tr>
   </tbody></table>


.. .. tabularcolumns:: |l|c|c|


.. .. list-table:: MySQL Backup Tool Feature Comparison
..    :header-rows: 1

..    * - Feature	
..      - Percona XtraBackup
..      - MySQL Enterprise Backup (InnoDB Hot Backup)
..    * - License
..      - GPL
..      - Proprietary
..    * - Price
..      - Free
..      - $5000 per server 
..    * - Open source
..      - |yes|
..      - 
..    * - Non-blocking
..      - |yes|
..      - |yes|
..    * - InnoDB backups
..      - |yes|
..      - |yes|
..    * - MyISAM backups [#f1]_
..      - |yes|
..      - |yes|
..    * - Compressed backups
..      - |yes|
..      - |yes|
..    * - Partial backups
..      - |yes|
..      - |yes|
..    * - Throttling [#f2]_
..      - |yes|
..      - |yes|
..    * - Point-in-time recovery support
..      - |yes|
..      - |yes|
..    * - Incremental backups
..      - |yes|
..      - |yes|
..    * - Parallel backups [#f3]_
..      -  |yes|
..      -
..    * - Streaming backups
..      - |yes|	
..      -
..    * - OS buffer optimizations [#f4]_
..      - |yes|	
..      -
..    * - Export individual tables
..      - |yes|	
..      -
..    * - Restore tables to a different server
..      - |yes|
..      -	
..    * - Analyze data & index files
..      - |yes|
..      -	
..    * - Familiar command-line behavior [#f5]_
..      - |yes|	
..      -

.. .. |yes| image:: check-yes.png

..  License	                              GPL	                 Proprietary
..  Price	                                      Free                  $5000 per server 
..  Open source	                              Yes	
..  Non-blocking	                              Yes                        Yes
..  InnoDB backups	                              Yes	                 Yes
..  MyISAM backups [#f1]_	                      Yes	                 Yes
..  Compressed backups	                      Yes	                 Yes
..  Partial backups                              Yes	                 Yes
..  Throttling [#f2]_                            Yes	                 Yes
..  Point-in-time recovery support	              Yes	                 Yes
..  Incremental backups	                      Yes	                 Yes
..  Parallel backups [#f3]_	              Yes	
..  Streaming backups	                      Yes	
..  OS buffer optimizations [#f4]_               Yes	
..  Export individual tables                     Yes	
..  Restore tables to a different server         Yes	
..  Analyze data & index files                   Yes	
..  Familiar command-line behavior [#f5]_        Yes	
.. ========================================   ===================   =========================

The above comparison is based on XtraBackup version 1.4 and MySQL Enterprise Backup version 3.5 on December 7, 2010. 


What are the features of Percona XtraBackup?
============================================

Here is a short list of |XtraBackup| features. See the documentation for more.

* Ceate hot |InnoDB| backups without pausing your database
* Make incremental backups of |MySQL|
* Stream compressed |MySQL| backups to another server
* Move tables between |MySQL| servers online
* Create new |MySQL| replication slaves easily
* Backup |MySQL| without adding load to the server



.. rubric:: Footnotes

.. [#note-1] |MyISAM| backups require a table lock.

.. [#note-2] |XtraBackup| performs throttling based on the number of IO operations per second. *MySQL Enterprise Backup* supports a configurable sleep time between operations.

.. [#note-3] |XtraBackup| tunes the operating system buffers to avoid swapping. See the documentation.

.. [#note-4] |XtraBackup| is linked against the |MySQL| client libraries, so it behaves the same as standard |MySQL| command-line programs. *MySQL Enterprise Backup* has its own command-line and configuration-file behaviors.


