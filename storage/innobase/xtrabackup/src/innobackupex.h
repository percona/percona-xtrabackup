/******************************************************
Copyright (c) 2011-2014 Percona LLC and/or its affiliates.

Declarations for innobackupex.cc

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*******************************************************/

#ifndef INNOBACKUPEX_H
#define INNOBACKUPEX_H

#define INNOBACKUPEX_BIN_NAME "innobackupex"

enum ibx_mode_t {
	IBX_MODE_BACKUP,
	IBX_MODE_APPLY_LOG,
	IBX_MODE_COPY_BACK,
	IBX_MODE_MOVE_BACK,
	IBX_MODE_DECRYPT_DECOMPRESS
};

extern ibx_mode_t ibx_mode;

bool
ibx_handle_options(int *argc, char ***argv);

bool
ibx_init();

void
ibx_cleanup();

bool
ibx_select_history();

bool
ibx_flush_changed_page_bitmaps();

bool
ibx_backup_start();

bool
ibx_backup_finish();

bool
ibx_apply_log_finish();

bool
ibx_copy_back();

bool
ibx_decrypt_decompress();

void
ibx_capture_tool_command(int argc, char **argv);

void
ibx_completed_ok();

#endif
