.. _user-manual:

==================================
 *Percona XtraBackup* User Manual
==================================

.. toctree::
   :maxdepth: 1
   :hidden:

   innobackupex/innobackupex_script
   xtrabackup_bin/xtrabackup_binary
   xbstream/xbstream
   xbcrypt/xbcrypt
   xbcloud/xbcloud
   how_xtrabackup_works

|Percona XtraBackup| is a set of following tools:

:doc:`innobackupex <innobackupex/innobackupex_script>`
    |innobackupex| is the symlink for |xtrabackup|. |innobackupex| still
    supports all features and syntax as 2.2 version did, but is now
    deprecated and will be removed in next major release.

:doc:`xtrabackup <xtrabackup_bin/xtrabackup_binary>`
    a compiled *C* binary that provides functionality to backup a whole |MySQL|
    database instance with |MyISAM|, |InnoDB|, and |XtraDB| tables.

:doc:`xbcrypt <xbcrypt/xbcrypt>`
   utility used for encrypting and decrypting backup files.

:doc:`xbstream <xbstream/xbstream>`
   utility that allows streaming and extracting files to/from the
   :term:`xbstream` format.

:doc:`xbcloud <xbcloud/xbcloud>`
   utility used for downloading and uploading full or part of |xbstream|
   archive from/to cloud.

After |Percona XtraBackup| 2.3 release the recommend way to take the backup is
using the |xtrabackup| script. More information on script options can be found
in :doc:`how to use xtrabackup <xtrabackup_bin/xtrabackup_binary>`.
