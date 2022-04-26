#include "file_utils.h"
#include "common.h"
#include "msg.h"
#include "my_dir.h"
#include "my_io.h"
#include "my_thread_local.h"

#if defined _WIN32 || defined __CYGWIN__ || defined __EMX__ || \
    defined __MSDOS__ || defined __DJGPP__
#define IS_DRIVE_LETTER(C) \
  (((C) >= 'A' && (C) <= 'Z') || ((C) >= 'a' && (C) <= 'z'))
#define HAS_DEVICE(Filename) \
  (IS_DRIVE_LETTER((Filename)[0]) && (Filename)[1] == FN_DEVCHAR)
#define FILE_SYSTEM_PREFIX_LEN(Filename) (HAS_DEVICE(Filename) ? 2 : 0)
#else
#define FILE_SYSTEM_PREFIX_LEN(Filename) ((void)(Filename), 0)
#endif

const char *safer_name_suffix(char const *file_name, int *prefix_len_out) {
  char const *p;

  /* Skip file system prefixes, leading file name components that contain
     "..", and leading slashes.  */

  int prefix_len = FILE_SYSTEM_PREFIX_LEN(file_name);

  // Remove ../
  for (p = file_name + prefix_len; *p;) {
    if (p[0] == '.' && p[1] == '.' && (is_directory_separator(p[2]) || !p[2]))
      prefix_len = p + 2 - file_name;

    do {
      char c = *p++;
      if (is_directory_separator(c)) break;
    } while (*p);
  }

  // Remove leading /
  for (p = file_name + prefix_len; is_directory_separator(*p); p++) continue;
  prefix_len = p - file_name;

  if (prefix_len) {
    msg("%s: removing leading '%.*s'.\n", my_progname, prefix_len, file_name);
  }

  /* Unlike tar, file_name is always a regular file, so p can't be null */

  *prefix_len_out = prefix_len;
  return p;
}

/************************************************************************
Check to see if a file exists.
Takes name of the file to check.
@return true if file exists. */
bool file_exists(const char *filename) {
  MY_STAT stat_arg;

  if (!my_stat(filename, &stat_arg, MYF(0))) {
    return (false);
  }

  return (true);
}

/************************************************************************
Return true if character if file separator */
bool is_path_separator(char c) { return is_directory_separator(c); }

/** Create directories recursively.
@return 0 if directories created successfully. */
int mkdirp(const char *pathname, int Flags, myf MyFlags) {
  char parent[PATH_MAX], *p;

  /* make a parent directory path */
  strncpy(parent, pathname, sizeof(parent));
  parent[sizeof(parent) - 1] = 0;

  for (p = parent + strlen(parent); !is_path_separator(*p) && p != parent; p--)
    ;

  *p = 0;

  /* try to create parent directory if it doesn't exist */
  if (!file_exists(parent)) {
    if (p != parent && mkdirp(parent, Flags, MyFlags) != 0) {
      return (-1);
    }
  }

  /* create this one if parent has been created */
  if (my_mkdir(pathname, Flags, MyFlags) == 0) {
    return (0);
  }

  /* if it already exists that is fine */
  if (my_errno() == EEXIST) {
    return (0);
  }

  return (-1);
}
