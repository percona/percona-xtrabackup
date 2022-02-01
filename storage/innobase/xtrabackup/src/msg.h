/******************************************************
Copyright (c) 2022 Percona LLC and/or its affiliates.

Common declarations for XtraBackup.

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

#ifndef XB_MSG_H
#define XB_MSG_H

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>

static inline int msg(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));
static inline int msg(const char *fmt, ...) {
  int result;
  va_list args;

  va_start(args, fmt);
  result = vfprintf(stderr, fmt, args);
  va_end(args);

  return result;
}

static inline int msg_ts(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));
static inline int msg_ts(const char *fmt, ...) {
  int result;
  time_t t = time(NULL);
  char date[100];
  char *line;
  va_list args;

  strftime(date, sizeof(date), "%y%m%d %H:%M:%S", localtime(&t));

  va_start(args, fmt);
  result = vasprintf(&line, fmt, args);
  va_end(args);

  if (result != -1) {
    result = fprintf(stderr, "%s %s", date, line);
    free(line);
  }

  return result;
}
#endif
