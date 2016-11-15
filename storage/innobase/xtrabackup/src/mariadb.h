/*****************************************************************************

Copyright (C) 2016, MariaDB Corporation. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/
/**************************************************//**
@file mariadb.h
Helper functions to check MariaDB 10.1 extended features.

Jan Lindström jan.lindstrom@mariadb.com
*******************************************************/

#include "univ.i"
#include "log0log.h"

bool mariadb_check_encryption(log_group_t* group);
bool mariadb_check_compression(const byte* page);
bool mariadb_check_tablespace_encryption(os_file_t fd, const char* name, ulint zip_size);

