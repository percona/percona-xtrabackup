/******************************************************
Copyright (c) 2011-2019 Percona LLC and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

*******************************************************/

#ifndef XTRABACKUP_BACKUP_COPY_H
#define XTRABACKUP_BACKUP_COPY_H

#include "datasink.h"
#include "xtrabackup_config.h"

struct Backup_context;

/* special files */
#define XTRABACKUP_SLAVE_INFO "xtrabackup_slave_info"
#define XTRABACKUP_GALERA_INFO "xtrabackup_galera_info"
#define XTRABACKUP_BINLOG_INFO "xtrabackup_binlog_info"
#define XTRABACKUP_INFO "xtrabackup_info"

bool backup_file_print(const char *filename, const char *message, int len);

bool backup_file_printf(const char *filename, const char *fmt, ...)
    __attribute__((format(printf, 2, 0)));

/************************************************************************
Return true if first and second arguments are the same path. */
bool equal_paths(const char *first, const char *second);

/* the purpose of file copied */
enum file_purpose_t {
  FILE_PURPOSE_DATAFILE,
  FILE_PURPOSE_REDO_LOG,
  FILE_PURPOSE_UNDO_LOG,
  FILE_PURPOSE_BINLOG,
  FILE_PURPOSE_OTHER
};

/************************************************************************
Write buffer into .ibd file and preserve it's sparsiness. */
bool write_ibd_buffer(ds_file_t *file, unsigned char *buf, size_t buf_len,
                      size_t page_size, size_t block_size);

/************************************************************************
Copy file for backup/restore.
@return true in case of success. */
bool copy_file(ds_ctxt_t *datasink, const char *src_file_path,
               const char *dst_file_path, uint thread_n,
               file_purpose_t file_purpose, ssize_t pos = -1);

/* Backup non-InnoDB data.
@return true if success. */
bool backup_start(Backup_context &context);

/* Finsh the backup. Release all locks. Write down backup metadata.
@return true if success. */
bool backup_finish(Backup_context &context);

bool apply_log_finish();
bool copy_back(int argc, char **argv);
bool decrypt_decompress();
#ifdef HAVE_VERSION_CHECK
void version_check();
#endif
bool is_path_separator(char);
bool directory_exists(const char *dir, bool create);
int mkdirp(const char *pathname, int Flags, myf MyFlags);

#endif
