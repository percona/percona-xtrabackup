/******************************************************
Copyright (c) 2013 Percona LLC and/or its affiliates.

Local datasink implementation for XtraBackup.

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

#include <my_base.h>
#include <mysql/service_mysql_alloc.h>
#include <mysys_err.h>
#include <atomic>
#include "common.h"
#include "datasink.h"

typedef struct {
  File fd;
} ds_stdout_file_t;

/* Per-datasink private state.  Used to carry the bytes_written tier-1
metric so xbstream-over-stdout backups can report backup_size the same
way ds_local does. */
struct ds_stdout_ctxt_t {
  std::atomic<unsigned long long> bytes_written{0};
};

static ds_ctxt_t *stdout_init(const char *root);
static ds_file_t *stdout_open(ds_ctxt_t *ctxt, const char *path,
                              MY_STAT *mystat);
static int stdout_write(ds_file_t *file, const void *buf, size_t len);
static int stdout_close(ds_file_t *file);
static void stdout_deinit(ds_ctxt_t *ctxt);
static void stdout_report_metrics(const ds_ctxt_t *ctxt,
                                  std::vector<ds_metric> &out);

datasink_t datasink_stdout = {
    &stdout_init,  &stdout_open,   &stdout_write,         nullptr,
    &stdout_close, &stdout_deinit, &stdout_report_metrics};

static ds_ctxt_t *stdout_init(const char *root) {
  ds_ctxt_t *ctxt;

  ctxt = static_cast<ds_ctxt_t *>(my_malloc(
      PSI_NOT_INSTRUMENTED, sizeof(ds_ctxt_t), MYF(MY_FAE | MY_ZEROFILL)));

  ctxt->ptr = new ds_stdout_ctxt_t{};
  ctxt->root = my_strdup(PSI_NOT_INSTRUMENTED, root, MYF(MY_FAE));

  return ctxt;
}

static ds_file_t *stdout_open(ds_ctxt_t *ctxt [[maybe_unused]],
                              const char *path __attribute__((unused)),
                              MY_STAT *mystat __attribute__((unused))) {
  ds_stdout_file_t *stdout_file;
  ds_file_t *file;
  size_t pathlen;
  const char *fullpath = "<STDOUT>";

  pathlen = strlen(fullpath) + 1;

  file = (ds_file_t *)my_malloc(
      PSI_NOT_INSTRUMENTED,
      sizeof(ds_file_t) + sizeof(ds_stdout_file_t) + pathlen, MYF(MY_FAE));
  stdout_file = (ds_stdout_file_t *)(file + 1);

#ifdef __WIN__
  setmode(fileno(stdout), _O_BINARY);
#endif

  stdout_file->fd = fileno(stdout);

  file->path = (char *)stdout_file + sizeof(ds_stdout_file_t);
  memcpy(file->path, fullpath, pathlen);

  file->ptr = stdout_file;

  return file;
}

static int stdout_write(ds_file_t *file, const void *buf, size_t len) {
  File fd = ((ds_stdout_file_t *)file->ptr)->fd;

  if (!my_write(fd, static_cast<const uchar *>(buf), len,
                MYF(MY_WME | MY_NABP))) {
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
    auto stdout_ctxt = static_cast<ds_stdout_ctxt_t *>(file->ctxt->ptr);
    stdout_ctxt->bytes_written.fetch_add(len, std::memory_order_relaxed);
    return 0;
  }

  return 1;
}

static int stdout_close(ds_file_t *file) {
  my_free(file);

  return 0;
}

static void stdout_deinit(ds_ctxt_t *ctxt) {
  delete static_cast<ds_stdout_ctxt_t *>(ctxt->ptr);
  my_free(ctxt->root);
  my_free(ctxt);
}

static void stdout_report_metrics(const ds_ctxt_t *ctxt,
                                  std::vector<ds_metric> &out) {
  const auto *c = static_cast<const ds_stdout_ctxt_t *>(ctxt->ptr);
  out.push_back(
      {"bytes_written", c->bytes_written.load(std::memory_order_relaxed)});
}
