# Copyright (c) 2006, 2024, Oracle and/or its affiliates.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# Copy man pages
<<<<<<< HEAD
FILE(GLOB MAN1 *.1)
FILE(GLOB MAN1_NDB ndb*.1)
FILE(GLOB MAN8 *.8)
FILE(GLOB MAN8_NDB ndb*.8)
||||||| 824e2b40640
SET(MAN1
  comp_err.1
  ibd2sdi.1
  innochecksum.1
  lz4_decompress.1
  my_print_defaults.1
  myisam_ftdump.1
  myisamchk.1
  myisamlog.1
  myisampack.1
  mysql.1
  mysql_config.1
  mysql_config_editor.1
  mysql_secure_installation.1
  mysql_ssl_rsa_setup.1
  mysql_tzinfo_to_sql.1
  mysql_upgrade.1
  mysqladmin.1
  mysqlbinlog.1
  mysqlcheck.1
  mysqldump.1
  mysqldumpslow.1
  mysqlimport.1
  mysqlman.1
  mysqlpump.1
  mysqlshow.1
  mysqlslap.1
  perror.1
  zlib_decompress.1
  )
=======
SET(MAN1
  comp_err.1
  ibd2sdi.1
  innochecksum.1
  my_print_defaults.1
  myisam_ftdump.1
  myisamchk.1
  myisamlog.1
  myisampack.1
  mysql.1
  mysql_config.1
  mysql_config_editor.1
  mysql_secure_installation.1
  mysql_tzinfo_to_sql.1
  mysqladmin.1
  mysqlbinlog.1
  mysqlcheck.1
  mysqldump.1
  mysqldumpslow.1
  mysqlimport.1
  mysqlman.1
  mysqlshow.1
  mysqlslap.1
  perror.1
  )
>>>>>>> mysql-8.4.0

IF(MAN1_NDB AND NOT WITH_NDB)
  LIST(REMOVE_ITEM MAN1 ${MAN1_NDB})
ENDIF()
INSTALL(FILES ${MAN1} DESTINATION ${INSTALL_MANDIR}/man1 COMPONENT ManPages)

IF(MAN8_NDB AND NOT WITH_NDB)
  LIST(REMOVE_ITEM MAN8 ${MAN8_NDB})
ENDIF()
INSTALL(FILES ${MAN8} DESTINATION ${INSTALL_MANDIR}/man8 COMPONENT ManPages)
