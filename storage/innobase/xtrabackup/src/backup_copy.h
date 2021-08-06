
#ifndef XTRABACKUP_BACKUP_COPY_H
#define XTRABACKUP_BACKUP_COPY_H

#include <my_global.h>
#include "datasink.h"

/* special files */
#define XTRABACKUP_SLAVE_INFO "xtrabackup_slave_info"
#define XTRABACKUP_GALERA_INFO "xtrabackup_galera_info"
#define XTRABACKUP_BINLOG_INFO "xtrabackup_binlog_info"
#define XTRABACKUP_INFO "xtrabackup_info"

bool
backup_file_print(const char *filename, const char *message, int len);

bool
backup_file_printf(const char *filename, const char *fmt, ...)
		__attribute__((format(printf, 2, 0)));

/************************************************************************
Return true if first and second arguments are the same path. */
bool
equal_paths(const char *first, const char *second);

/************************************************************************
Copy file for backup/restore.
@return true in case of success. */
bool
copy_file(ds_ctxt_t *datasink,
	  const char *src_file_path,
	  const char *dst_file_path,
	  uint thread_n);

bool
backup_start();
bool
backup_finish();
bool
apply_log_finish();
bool
copy_back(int argc, char **argv);
bool
decrypt_decompress();
#ifdef HAVE_VERSION_CHECK
void
version_check();
#endif
bool
is_path_separator(char);
bool
file_exists(const char *filename, bool check_if_file=false);
bool
directory_exists(const char *dir, bool create);

#endif
