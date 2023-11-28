/******************************************************
Copyright (c) 2019,2023 Percona LLC and/or its affiliates.

Redo log handling.

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

#include "redo_log.h"

#include <dict0dict.h>
#include <log0chkp.h>
#include <log0log.h>
#include <ut0ut.h>
#include <univ.i>

#include <sstream>

#include "backup_copy.h"
#include "backup_mysql.h"
#include "common.h"
#include "file_utils.h"
#include "log0encryption.h"
#include "os0event.h"
#include "sql_thd_internal_api.h"
#include "xb0xb.h"
#include "xtrabackup.h"

extern ds_ctxt_t *ds_redo;
/* first block of redo archive file which is all zero in 8.0.22  */
constexpr size_t HEADER_BLOCK_SIZE = 4096;
static bool archive_first_block_zero = false;
std::atomic<bool> Redo_Log_Reader::m_error;
IF_DEBUG(bool force_reopen = false;);

Redo_Log_Reader::Redo_Log_Reader() {
  log_hdr_buf.alloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                            ut::Count{LOG_FILE_HDR_SIZE});
  log_buf.alloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                        ut::Count{redo_log_read_buffer_size});

  m_error = false;
}

bool Redo_Log_Reader::find_start_checkpoint_lsn() {
  /* Look for the latest checkpoint */
  Log_checkpoint_location checkpoint;

  if (!recv_find_max_checkpoint(*log_sys, checkpoint)) {
    xb::error() << "recv_find_max_checkpoint() failed.";
    return (false);
  }

  /* Redo log file header is never encrypted. */
  Encryption_metadata unused_encryption_metadata;

  auto file_handle =
      Log_file::open(log_sys->m_files_ctx, checkpoint.m_checkpoint_file_id,
                     Log_file_access_mode::READ_ONLY,
                     unused_encryption_metadata, Log_file_type::NORMAL);
  if (!file_handle.is_open()) {
    xb::error() << "Failed to open redo log file id "
                << checkpoint.m_checkpoint_file_id;
    return false;
  }
  log_scanned_lsn = checkpoint.m_checkpoint_lsn;
  checkpoint_lsn_start = checkpoint.m_checkpoint_lsn;

  debug_sync_point("stop_before_copy_log_hdr");

  /* Copy the header into log_hdr_buf */
  file_handle.read(0, LOG_FILE_HDR_SIZE, log_hdr_buf);

  /* update the headers to store the current checkpoint LSN */
  update_log_temp_checkpoint(log_hdr_buf, checkpoint_lsn_start);

  return (true);
}

byte *Redo_Log_Reader::get_header() const { return log_hdr_buf; }

byte *Redo_Log_Reader::get_buffer() const { return log_buf; }

lsn_t Redo_Log_Reader::get_scanned_lsn() const { return (log_scanned_lsn); }

lsn_t Redo_Log_Reader::get_contiguous_lsn() const {
  return ut_uint64_align_down(log_scanned_lsn, OS_FILE_LOG_BLOCK_SIZE);
}

lsn_t Redo_Log_Reader::get_start_checkpoint_lsn() const {
  return (checkpoint_lsn_start);
}

bool Redo_Log_Reader::is_error() const { return (m_error); }

/** scan redo log files form server directory and update log.m_files.
@param[in,out]  desired_lsn             LSN that triggered file reopening.
if LSN == 0, PXB opens the redo log file and checks if the redo is disabled or
not.
@return true if re-open was sucessful */

static bool reopen_log_files(lsn_t desired_lsn) {
  log_t &log = *log_sys;
  Log_files_dict files{log.m_files_ctx};
  Log_format format;
  std::string creator_name;
  Log_flags log_flags;
  Log_uuid log_uuid;

  auto res = log_files_find_and_analyze(
      srv_read_only_mode, log.m_encryption_metadata, files, format,
      creator_name, log_flags, log_uuid);

  if (desired_lsn == 0 && res != Log_files_find_result::FOUND_VALID_FILES) {
    return false;
  }

  if (res != Log_files_find_result::FOUND_VALID_FILES) {
    xb::error()
        << "reopen_log_files did not find valid files. Possibly the "
           "redo log files have been removed by the server. Consider "
           "using redo log archiving via --redo-log-arch-dir=name or "
           "registering redo log consumer via --register-redo-log-consumer.";
    return false;
  }

  log.m_format = format;
  log.m_creator_name = creator_name;
  log.m_log_flags = log_flags;
  log.m_log_uuid = log_uuid;
  log.m_files = std::move(files);
  if (desired_lsn == 0) {
    return true;
  }
  auto file = log.m_files.find(desired_lsn);
  if (file == log.m_files.end()) {
    xb::error() << "could not find redo log file with LSN " << desired_lsn;
    return false;
  }
  if (log_encryption_read(log, *file) != DB_SUCCESS) {
    xb::error() << "log_encryption_read failed on file ID " << file->m_id;
    return (false);
  };
  return true;
}

lsn_t Redo_Log_Reader::read_log_seg(log_t &log, byte *buf, lsn_t start_lsn,
                                    lsn_t end_lsn) {
  ut_a(start_lsn < end_lsn);

  // update the in-memory structure log files by scanning
  if (IF_DEBUG((force_reopen && !reopen_log_files(start_lsn)) ||)(
          log.m_files.find(start_lsn) == log.m_files.end() &&
          !reopen_log_files(start_lsn))) {
    m_error = true;
    return 0;
  }

  DBUG_EXECUTE_IF(
      "xtrabackup_reopen_files_after_catchup",
      if (xtrabackup_debug_sync != nullptr &&
          strstr(xtrabackup_debug_sync, "log_files_find_and_analyze") !=
              nullptr) {
        *const_cast<const char **>(&xtrabackup_debug_sync) = nullptr;
        DBUG_SET("-d,xtrabackup_reopen_files_after_catchup");
        force_reopen = false;
      });

  auto file = log.m_files.find(start_lsn);

  ut_ad(file != log.m_files.end());

  auto file_handle = file->open(Log_file_access_mode::READ_ONLY);

  if (!file_handle.is_open()) {
    // file not found
    m_error = true;
    return 0;
  }

  do {
    os_offset_t source_offset = file->offset(start_lsn);
    ut_a(end_lsn - start_lsn <= ULINT_MAX);

    os_offset_t len = end_lsn - start_lsn;

    ut_ad(len != 0);

    bool switch_to_next_file = false;

    if (source_offset + len > file->m_size_in_bytes) {
      /* If the above condition is true then len
      (which is unsigned) is > the expression below,
      so the typecast is ok */
      ut_a(file->m_size_in_bytes > source_offset);
      len = file->m_size_in_bytes - source_offset;
      switch_to_next_file = true;
    }

    ++log.n_log_ios;

    const dberr_t err =
        log_data_blocks_read(file_handle, source_offset, len, buf);
    ut_a(err == DB_SUCCESS);

    start_lsn += len;
    buf += len;

    if (switch_to_next_file) {
      auto next_id = file->next_id();

      const auto next_file = log.m_files.file(next_id);

      if (next_file == log.m_files.end() || !next_file->contains(start_lsn)) {
        return start_lsn;
      }

      file_handle.close();

      file = next_file;

      file_handle = file->open(Log_file_access_mode::READ_ONLY);
      ut_a(file_handle.is_open());
    }

  } while (start_lsn != end_lsn);

  ut_a(start_lsn == end_lsn);

  return end_lsn;
}

ssize_t Redo_Log_Reader::read_logfile(bool is_last, bool *finished) {
  log_t &log = *log_sys;

  lsn_t start_lsn =
      ut_uint64_align_down(log_scanned_lsn, OS_FILE_LOG_BLOCK_SIZE);
  lsn_t scanned_lsn = start_lsn;

  size_t len = 0;

  *finished = false;

  while (!*finished && len <= redo_log_read_buffer_size - RECV_SCAN_SIZE) {
    const lsn_t end_lsn =
        read_log_seg(log, log_buf + len, start_lsn, start_lsn + RECV_SCAN_SIZE);

    auto size = scan_log_recs(log_buf + len, is_last, start_lsn, &scanned_lsn,
                              log_scanned_lsn, finished);
    if (end_lsn == start_lsn) {
      break;
    }

    if (size < 0 || end_lsn == 0) {
      xb::error() << "read_logfile() failed.";
      return (-1);
    }

    start_lsn = end_lsn;
    len += size;
  }

  log_scanned_lsn = scanned_lsn;

  return (len);
}

ssize_t Redo_Log_Reader::scan_log_recs(byte *buf, bool is_last, lsn_t start_lsn,
                                       lsn_t *read_upto_lsn,
                                       lsn_t checkpoint_lsn, bool *finished) {
  lsn_t scanned_lsn{start_lsn};
  const byte *log_block{buf};

  *finished = false;

  ulint scanned_epoch_no{0};

  while (log_block < buf + RECV_SCAN_SIZE && !*finished) {
    Log_data_block_header block_header;
    log_data_block_header_deserialize(log_block, block_header);

    const uint32_t expected_hdr_no =
        log_block_convert_lsn_to_hdr_no(scanned_lsn);

    auto checksum_is_ok = log_block_checksum_is_ok(log_block);

    if (block_header.m_hdr_no != expected_hdr_no && checksum_is_ok) {
      /* old log block, do nothing */
      if (block_header.m_hdr_no < expected_hdr_no) {
        *finished = true;
        break;
      }
      xb::error() << "log block numbers mismatch:";
      xb::error() << "expected log block no. " << expected_hdr_no
                  << ", but got no. " << block_header.m_hdr_no
                  << " from the log file.";

      return (-1);

    } else if (!checksum_is_ok) {
      /* Garbage or an incompletely written log block */
      xb::warn() << "Log block checksum mismatch (block no "
                 << block_header.m_hdr_no << " at lsn " << scanned_lsn
                 << "): expected " << log_block_get_checksum(log_block)
                 << ", calculated checksum "
                 << log_block_calc_checksum(log_block)
                 << " block epoch no: " << block_header.m_epoch_no;
      xb::warn() << "this is possible when the "
                    "log block has not been fully written by the "
                    "server, will retry later.";
      *finished = true;
      break;
    }

    if (scanned_epoch_no > 0 &&
        !log_block_epoch_no_is_valid(block_header.m_epoch_no,
                                     scanned_epoch_no)) {
      /* Garbage from a log buffer flush which was made
      before the most recent database recovery */

      *finished = true;
      break;
    }
    const auto data_len = log_block_get_data_len(log_block);

    scanned_lsn = scanned_lsn + data_len;
    scanned_epoch_no = block_header.m_epoch_no;

    if (data_len < OS_FILE_LOG_BLOCK_SIZE) {
      /* Log data for this group ends here */

      *finished = true;
    } else {
      log_block += OS_FILE_LOG_BLOCK_SIZE;
    }
  }

  *read_upto_lsn = scanned_lsn;

  ssize_t write_size;

  if (!*finished) {
    write_size = RECV_SCAN_SIZE;
  } else {
    write_size =
        ut_uint64_align_up(scanned_lsn, OS_FILE_LOG_BLOCK_SIZE) - start_lsn;
    if (!is_last && scanned_lsn % OS_FILE_LOG_BLOCK_SIZE) {
      write_size -= OS_FILE_LOG_BLOCK_SIZE;
    }
  }

  return (write_size);
}

void Redo_Log_Reader::seek_logfile(lsn_t lsn) { log_scanned_lsn = lsn; }

lsn_t Redo_Log_Parser::get_last_parsed_lsn() const {
  return (last_parsed_lsn.load());
}

bool Redo_Log_Parser::parse_log(const byte *buf, size_t len, lsn_t start_lsn) {
  const byte *log_block = buf;
  lsn_t scanned_lsn = start_lsn;
  bool more_data = false;

  do {
    Log_data_block_header block_header;
    log_data_block_header_deserialize(log_block, block_header);
    ulint data_len = log_block_get_data_len(log_block);

    if (!recv_sys->parse_start_lsn && block_header.m_first_rec_group > 0) {
      /* We found a point from which to start the parsing
      of log records */

      recv_sys->parse_start_lsn = scanned_lsn + block_header.m_first_rec_group;

      xb::info() << "Starting to parse redo log at lsn = "
                 << recv_sys->parse_start_lsn;

      if (recv_sys->parse_start_lsn < recv_sys->checkpoint_lsn) {
        /* We start to parse log records even before
        checkpoint_lsn, from the beginning of the log
        block which contains the checkpoint_lsn.

        That's because the first group of log records
        in the log block, starts before checkpoint_lsn,
        and checkpoint_lsn could potentially point to
        the middle of some log record. We need to find
        the first group of log records that starts at
        or after checkpoint_lsn. This could be only
        achieved by traversing all groups of log records
        that start within the log block since the first
        one (to discover their beginnings we need to
        parse them). However, we don't want to report
        missing tablespaces for space_id in log records
        before checkpoint_lsn. Hence we need to ignore
        those records and that's why we need a counter
        of bytes to ignore. */

        recv_sys->bytes_to_ignore_before_checkpoint =
            recv_sys->checkpoint_lsn - recv_sys->parse_start_lsn;

        ut_a(recv_sys->bytes_to_ignore_before_checkpoint <=
             OS_FILE_LOG_BLOCK_SIZE - LOG_BLOCK_HDR_SIZE);

        ut_a(recv_sys->checkpoint_lsn % OS_FILE_LOG_BLOCK_SIZE +
                 LOG_BLOCK_TRL_SIZE <
             OS_FILE_LOG_BLOCK_SIZE);

        ut_a(recv_sys->parse_start_lsn % OS_FILE_LOG_BLOCK_SIZE >=
             LOG_BLOCK_HDR_SIZE);
      }

      recv_sys->scanned_lsn = recv_sys->parse_start_lsn;
      recv_sys->recovered_lsn = recv_sys->parse_start_lsn;
    }

    scanned_lsn += data_len;

    if (scanned_lsn > recv_sys->scanned_lsn) {
      /* We were able to find more log data: add it to the
      parsing buffer if parse_start_lsn is already
      non-zero */

      if (recv_sys->len + 4 * OS_FILE_LOG_BLOCK_SIZE >= recv_sys->buf_len) {
        if (!recv_sys_resize_buf()) {
          recv_sys->found_corrupt_log = true;
        }
      }

      if (!recv_sys->found_corrupt_log) {
        more_data =
            recv_sys_add_to_parsing_buf(log_block, scanned_lsn, data_len);
      }

      recv_sys->scanned_lsn = scanned_lsn;

      recv_sys->scanned_epoch_no = block_header.m_epoch_no;
    }

    if (data_len < OS_FILE_LOG_BLOCK_SIZE) {
      /* Log data for this group ends here */

      break;

    } else {
      log_block += OS_FILE_LOG_BLOCK_SIZE;
    }

  } while (log_block < buf + len);

  if (more_data && !recv_sys->found_corrupt_log) {
    /* Try to parse more log records */

    recv_parse_log_recs();
    /* update parsed lsn after we have completed parsing */
    last_parsed_lsn.store(scanned_lsn);

    if (recv_sys->recovered_offset > recv_sys->buf_len / 4) {
      /* Move parsing buffer data to the buffer start */

      recv_reset_buffer();
    }
  }

  return (true);
}

Redo_Log_Writer::Redo_Log_Writer() {
  scratch_buf.alloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                            ut::Count{16 * 1024 * 1024});
}

bool Redo_Log_Writer::create_logfile(const char *name) {
  MY_STAT stat_info;

  memset(&stat_info, 0, sizeof(MY_STAT));
  log_file = ds_open(ds_redo, XB_LOG_FILENAME, &stat_info);

  if (log_file == NULL) {
    xb::error() << "failed to open the target stream for "
                << SQUOTE(XB_LOG_FILENAME);
    return (false);
  }

  return (true);
}

bool Redo_Log_Writer::write_header(byte *hdr) {
  const auto creator = "xtrabkup ";
  const auto creator_size = sizeof "xtrabkup " - 1;

  memcpy(hdr + LOG_HEADER_CREATOR, creator, creator_size);
  ut_sprintf_timestamp(reinterpret_cast<char *>(hdr) +
                       (LOG_HEADER_CREATOR + creator_size));

  if (ds_write(log_file, hdr, LOG_FILE_HDR_SIZE)) {
    xb::error() << "write to logfile failed";
    return (false);
  }

  return (true);
}

bool Redo_Log_Writer::close_logfile() {
  if (ds_close(log_file) != 0) {
    xb::error() << "failed to close logfile";
    return (false);
  }
  return (true);
}


bool Redo_Log_Writer::write_buffer(byte *buf, size_t len) {
  byte *write_buf = buf;

  if (srv_redo_log_encrypt) {
    IORequest req_type(IORequest::WRITE);
    req_type.get_encryption_info().set(log_sys->m_encryption_metadata);
    ut_ad(req_type.is_encrypted());

    Encryption encryption(req_type.encryption_algorithm());

    if (!encryption.encrypt_log(buf, len, scratch_buf)) {
      xb::error() << "Failed to encrypt redo log";
      return false;
    }
    write_buf = scratch_buf;
  }
  if (ds_write(log_file, write_buf, len)) {
    xb::error() << "write to logfile failed";
    return (false);
  }

  return (true);
}

Archived_Redo_Log_Reader::Archived_Redo_Log_Reader() {
  log_buf.alloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                        ut::Count{redo_log_read_buffer_size});
  scratch_buf.alloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                            ut::Count{UNIV_PAGE_SIZE_MAX});
}

void Archived_Redo_Log_Reader::set_fd(File fd) { file = fd; }

ssize_t Archived_Redo_Log_Reader::read_logfile(bool *finished) {
  auto read_size =
      ut_uint64_align_down(redo_log_read_buffer_size, OS_FILE_LOG_BLOCK_SIZE);

  auto len = my_read(file, log_buf, read_size, MYF(MY_WME | MY_FULL_IO));
  if (len == MY_FILE_ERROR) {
    return (-1);
  }

  ut_a(len % OS_FILE_LOG_BLOCK_SIZE == 0);

  log_scanned_lsn += len;

  if (len < redo_log_read_buffer_size) {
    *finished = true;
  }

  if (srv_redo_log_encrypt) {
    IORequest req_type(IORequest::READ);
    req_type.get_encryption_info().set(log_sys->m_encryption_metadata);
    Encryption encryption(req_type.encryption_algorithm());
    auto err = encryption.decrypt_log(log_buf, len);
    ut_a(err == DB_SUCCESS);
  }

  for (const byte *log_block = log_buf; log_block < log_buf + len;
       log_block += OS_FILE_LOG_BLOCK_SIZE) {
    ut_a(log_block_checksum_is_ok(log_block));
  }

  return (len);
}

bool Archived_Redo_Log_Reader::seek_logfile(lsn_t lsn) {
  lsn = ut_uint64_align_down(lsn, OS_FILE_LOG_BLOCK_SIZE);
  if (lsn < archive_start_lsn) {
    DBUG_PRINT("archive",
               ("Archived redo start lsn " LSN_PF " than request lsn " LSN_PF,
                archive_start_lsn, lsn));
    return (false);
  }

  auto pos = lsn - archive_start_lsn;
  if (archive_first_block_zero) pos += HEADER_BLOCK_SIZE;

  my_seek(file, 0, MY_SEEK_END, MYF(MY_FAE));
  auto file_size = my_tell(file, MYF(MY_FAE));

  if (file_size < pos) {
    DBUG_PRINT("archive", ("Archived redo file_size: %llu pos seeked: " LSN_PF,
                           file_size, pos));
    my_seek(file, 0, MY_SEEK_END, MYF(MY_FAE));
    file_size = my_tell(file, MYF(MY_FAE));

    return (false);
  }

  my_seek(file, pos, MY_SEEK_SET, MYF(MY_FAE));

  log_scanned_lsn = lsn;

  return (true);
}

void Archived_Redo_Log_Reader::set_start_lsn(lsn_t lsn) {
  ut_a(lsn % OS_FILE_LOG_BLOCK_SIZE == 0);

  archive_start_lsn = lsn;
  log_scanned_lsn = lsn + OS_FILE_LOG_BLOCK_SIZE;
}

byte *Archived_Redo_Log_Reader::get_buffer() const { return (log_buf); }

lsn_t Archived_Redo_Log_Reader::get_contiguous_lsn() const {
  return (log_scanned_lsn);
}

Archived_Redo_Log_Monitor::Archived_Redo_Log_Monitor() {
  event = os_event_create();
}

Archived_Redo_Log_Monitor::~Archived_Redo_Log_Monitor() {}

void Archived_Redo_Log_Monitor::close() { os_event_destroy(event); }

void Archived_Redo_Log_Monitor::start() {
  thread = os_thread_create(PFS_NOT_INSTRUMENTED, 0, [this] { thread_func(); });
  thread.start();
  os_event_wait_time_low(event, std::chrono::milliseconds{100}, 0);
  debug_sync_point("stop_before_redo_archive");
}

void Archived_Redo_Log_Monitor::stop() {
  stopped = true;
  os_event_set(event);
  thread.join();
}

Archived_Redo_Log_Reader &Archived_Redo_Log_Monitor::get_reader() {
  return reader;
}

bool Archived_Redo_Log_Monitor::is_ready() const { return ready; }

uint32_t Archived_Redo_Log_Monitor::get_first_log_block_no() const {
  return first_log_block_no;
}

uint32_t Archived_Redo_Log_Monitor::get_first_log_block_checksum() const {
  return first_log_block_checksum;
}

void Archived_Redo_Log_Monitor::skip_for_block(lsn_t lsn,
                                               const byte *redo_buf) {
  bool finished = false;
  lsn_t bytes_read = OS_FILE_LOG_BLOCK_SIZE;

  auto redo_block_no = log_block_get_hdr_no(redo_buf);
  auto redo_block_checksum = log_block_get_checksum(redo_buf);
  auto redo_block_len = log_block_get_data_len(redo_buf);

  while (true) {
    auto len = reader.read_logfile(&finished);
    for (auto ptr = reader.get_buffer(); ptr < reader.get_buffer() + len;
         ptr += OS_FILE_LOG_BLOCK_SIZE, bytes_read += OS_FILE_LOG_BLOCK_SIZE) {
      auto arch_block_no = log_block_get_hdr_no(ptr);
      auto arch_block_checksum = log_block_get_checksum(ptr);
      auto arch_block_len = log_block_get_data_len(ptr);
      /* When checksum of redo and archive blocks are different, allow PXB to
      switch to archive if the data length of blocks is different. This can
      happen when the last block is partially filled in redolog and when it
      reaches the archive file, the same block could be filled more */
  if (redo_block_no == arch_block_no &&
      (redo_block_checksum == arch_block_checksum ||
       redo_block_len != arch_block_len)) {
    reader.set_start_lsn(lsn - bytes_read);
    xb::info() << "Archived redo log has caught up at lsn "
               << reader.get_start_lsn();
    return;
      }
    }
    if (finished) {
      xb::info() << "Finished reading archive, did not find a matching block";
      return;
    }
  }
}

void Archived_Redo_Log_Monitor::parse_archive_dirs(const std::string &s) {
  std::stringstream ss(s);
  std::string item;

  while (std::getline(ss, item, ';')) {
    const auto p = item.find(':');
    if (p != std::string::npos) {
      auto label = item.substr(0, p);
      auto dir = item.substr(p + 1);
      archived_dirs.emplace(label, dir);
    }
  }
}

void Archived_Redo_Log_Monitor::archive_error_handle(MYSQL *mysql) {
  if (xb_has_set_redo_log_arch) {
    xb_mysql_query(mysql, "SET GLOBAL innodb_redo_log_archive_dirs = NULL;",
                   false, true);
  }
}

void Archived_Redo_Log_Monitor::thread_func() {
  my_thread_init();

  stopped = false;
  ready = false;
  xb_has_set_redo_log_arch = false;

  auto mysql = xb_mysql_connect();
  if (mysql == nullptr) {
    my_thread_end();
    return;
  }

  char *redo_log_archive_dirs = nullptr;
  char *server_uuid = nullptr;

  mysql_variable vars[] = {
      {"innodb_redo_log_archive_dirs", &redo_log_archive_dirs},
      {"server_uuid", &server_uuid},
      {nullptr, nullptr}};

  struct {
    std::string filename;
    std::string label;
    std::string dir;
    std::string subdir;
  } archive;

  read_mysql_variables(mysql, "SHOW VARIABLES", vars, true);

  if (redo_log_archive_dirs == nullptr || *redo_log_archive_dirs == 0) {
    std::ostringstream query;
    if (xtrabackup_redo_log_arch_dir) {
      xb::info() << "Setting up Redo Log Archiving to "
                 << xtrabackup_redo_log_arch_dir;
      query << "SET GLOBAL innodb_redo_log_archive_dirs ="
            << "\"" << xtrabackup_redo_log_arch_dir << "\"";
      xb_mysql_query(mysql, query.str().c_str(), false, true);
      xb_has_set_redo_log_arch = true;
    } else {
      xb::info() << "Redo Log Archiving is not set up.";
      free_mysql_variables(vars);
      mysql_close(mysql);
      my_thread_end();
      return;
    }
  }

  free_mysql_variables(vars);
  read_mysql_variables(mysql, "SHOW VARIABLES", vars, true);
  parse_archive_dirs(redo_log_archive_dirs);

  if (archived_dirs.empty()) {
    xb::info() << "Redo Log Archiving directory is empty.";
    archive_error_handle(mysql);
    free_mysql_variables(vars);
    mysql_close(mysql);
    my_thread_end();
    return;
  }

  xb::info() << "xtrabackup redo_log_arch_dir is set to "
             << redo_log_archive_dirs;
  for (const auto &dir : archived_dirs) {
    /* try to create a directory */
    using namespace std::chrono;
    archive.subdir = std::to_string(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch())
            .count());

    archive.dir = dir.second;
    archive.dir.push_back(OS_PATH_SEPARATOR);
    archive.dir.append(archive.subdir);

    if (mkdirp(archive.dir.c_str(), 0750, MYF(MY_WME)) != 0) {
      continue;
    }

    archive.filename = archive.dir;
    archive.filename.push_back(OS_PATH_SEPARATOR);
    archive.filename.append("archive.");
    archive.filename.append(server_uuid);
    archive.filename.append(".000001.log");

    archive.label = dir.first;

    break;
  }

  free_mysql_variables(vars);

  std::string start_query("select innodb_redo_log_archive_start(");

  start_query.append("'");
  start_query.append(archive.label);
  start_query.append("', '");
  start_query.append(archive.subdir);
  start_query.append("')");

  auto res = xb_mysql_query(mysql, start_query.c_str(), true, false);

  if (res == nullptr) {
    xb::info() << "Redo Log Archiving is not used.";
    archive_error_handle(mysql);
    mysql_close(mysql);
    my_thread_end();
    return;
  }

  mysql_free_result(res);

  xb::info() << "Waiting for archive file " << SQUOTE(archive.filename.c_str());

  /* wait for archive file to appear */
  while (!stopped) {
    bool exists;
    os_file_type_t type;
    if (!os_file_status(archive.filename.c_str(), &exists, &type)) {
      xb::error() << "cannot stat file " << SQUOTE(archive.filename.c_str());
      break;
    }
    if (exists) {
      break;
    }
    os_event_wait_time_low(event, std::chrono::milliseconds{100}, 0);
    os_event_reset(event);
  }

  File file{-1};
  if (!stopped) {
    file = my_open(archive.filename.c_str(), O_RDONLY, MYF(MY_WME));
    if (file < 0) {
      xb::error() << "cannot open " << SQUOTE(archive.filename.c_str());
      archive_error_handle(mysql);
      mysql_close(mysql);
      my_thread_end();
      return;
    }

    ut::aligned_array_pointer<byte, UNIV_PAGE_SIZE_MAX> buf;
    buf.alloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                      ut::Count{OS_FILE_LOG_BLOCK_SIZE});

    size_t hdr_len = OS_FILE_LOG_BLOCK_SIZE;
    while (hdr_len > 0 && !stopped) {
      size_t n_read = my_read(file, buf, hdr_len, MYF(MY_WME));
      if (n_read == MY_FILE_ERROR) {
        xb::error() << "cannot read from " << SQUOTE(archive.filename.c_str());
        archive_error_handle(mysql);
        mysql_close(mysql);
        my_thread_end();
        return;
      }
      os_event_wait_time_low(event, std::chrono::milliseconds{100}, 0);
      os_event_reset(event);
      hdr_len -= n_read;
    }

    /* from 8.0.22 onwards, the first block of redo archive file is all zeros */
    if (log_block_get_checksum(buf) == 0) {
      archive_first_block_zero = true;
      hdr_len = HEADER_BLOCK_SIZE - OS_FILE_LOG_BLOCK_SIZE;
      ut::aligned_array_pointer<byte, UNIV_PAGE_SIZE_MAX> buf2;
      buf2.alloc_withkey(UT_NEW_THIS_FILE_PSI_KEY,
                         ut::Count{HEADER_BLOCK_SIZE - OS_FILE_LOG_BLOCK_SIZE});
      /* Read remaining header of length (4096 - 512) bytes */
      while (hdr_len > 0 && !stopped) {
        size_t n_read = my_read(file, buf2, hdr_len, MYF(MY_WME));
        if (n_read == MY_FILE_ERROR) {
          xb::error() << "cannot read from " << archive.filename.c_str();
          archive_error_handle(mysql);
          mysql_close(mysql);
          my_thread_end();
          return;
        }
        os_event_wait_time_low(event, std::chrono::milliseconds{100}, 0);
        os_event_reset(event);
        hdr_len -= n_read;
      }

      hdr_len = OS_FILE_LOG_BLOCK_SIZE;
      /* Read the actual data in the archive file */
      while (hdr_len > 0 && !stopped) {
        size_t n_read = my_read(file, buf, hdr_len, MYF(MY_WME));
        if (n_read == MY_FILE_ERROR) {
          xb::error() << "cannot read from " << archive.filename.c_str();
          archive_error_handle(mysql);
          mysql_close(mysql);
          my_thread_end();
          return;
        }
        os_event_wait_time_low(event, std::chrono::milliseconds{100}, 0);
        os_event_reset(event);
        hdr_len -= n_read;
      }
      if (srv_redo_log_encrypt) {
        IORequest req_type(IORequest::READ);
        req_type.get_encryption_info().set(log_sys->m_encryption_metadata);
        Encryption encryption(req_type.encryption_algorithm());
        auto err = encryption.decrypt_log(buf, hdr_len);
        ut_a(err == DB_SUCCESS);
      }
    }

    first_log_block_no = log_block_get_hdr_no(buf);
    first_log_block_checksum = log_block_get_checksum(buf);
    ready = true;
  }

  if (ready && !stopped) {
    xb::info() << "Redo Log Archive " << SQUOTE(archive.filename.c_str())
               << " found. First log block is " << first_log_block_no
               << ", checksum is " << first_log_block_checksum;
    reader.set_fd(file);
  }

  while (!stopped) {
    os_event_wait_time_low(event, std::chrono::milliseconds{100}, 0);
    os_event_reset(event);
  }

  xb_mysql_query(mysql, "SELECT innodb_redo_log_archive_stop()", false);
  /*Return the configuration back to the original value*/
  if (xb_has_set_redo_log_arch) {
    xb_mysql_query(mysql, "SET GLOBAL innodb_redo_log_archive_dirs = NULL;",
                   false, true);
  }
  unlink(archive.filename.c_str());
  rmdir(archive.dir.c_str());
  mysql_close(mysql);
  my_thread_end();
}

bool Redo_Log_Data_Manager::init() {
  srv_redo_log_capacity = srv_redo_log_capacity_used = 0;
  error = true;
  event = os_event_create();

  thread = os_thread_create(PFS_NOT_INSTRUMENTED, 0, [this] { copy_func(); });

  lsn_t flushed_lsn = LOG_START_LSN + LOG_BLOCK_HDR_SIZE;

  if (xtrabackup_register_redo_log_consumer) {
    if (!redo_log_consumer.check()) {
      xtrabackup_register_redo_log_consumer = false;
    } else {
      redo_log_consumer_cnx = xb_mysql_connect();
      if (redo_log_consumer_cnx == nullptr) {
        xtrabackup_register_redo_log_consumer = false;
        return (false);
      }
      /* Consumer queries might not work on some sql_mode.
       * Forcing it to be empty.
       */
      xb_mysql_query(redo_log_consumer_cnx, "SET sql_mode=''", false, true);
      redo_log_consumer.init(redo_log_consumer_cnx);
      redo_log_consumer_can_advance.store(true);
    }
  }

  if (log_sys_init(false, flushed_lsn, flushed_lsn) != DB_SUCCESS) {
    return (false);
  }
  lsn_t checkpoint_lsn;
  Log_checkpoint_location checkpoint;
  if (!recv_find_max_checkpoint(*log_sys, checkpoint)) {
    xb::error() << " recv_find_max_checkpoint() failed.";
    return (false);
  }
  checkpoint_lsn = checkpoint.m_checkpoint_lsn;
  auto file = log_sys->m_files.find(checkpoint_lsn);
  if (file == log_sys->m_files.end()) {
    xb::error() << " Cannot find file with checkpoint " << checkpoint_lsn;
    return (false);
  }
  if (log_encryption_read(*log_sys, *file) != DB_SUCCESS) {
    xb::error() << "log_encryption_read failed on file ID " << file->m_id;
    return (false);
  };

  ut_a(log_sys != nullptr);

  recv_sys_create();
  recv_sys_init();

  ut_a(log_sys != nullptr);

  archived_log_state = ARCHIVED_LOG_NONE;
  archived_log_monitor.start();

  error = false;

  return (true);
}

bool Redo_Log_Data_Manager::start() {
  error = true;

  if (!reader.find_start_checkpoint_lsn()) {
    return (false);
  }

  if (!writer.create_logfile(XB_LOG_FILENAME)) {
    return (false);
  }

  if (!writer.write_header(reader.get_header())) {
    return (false);
  }

  start_checkpoint_lsn = reader.get_start_checkpoint_lsn();
  last_checkpoint_lsn = start_checkpoint_lsn;
  xtrabackup_start_checkpoint = start_checkpoint_lsn;
  stop_lsn = 0;

  if (opt_lock_ddl_per_table) {
    mdl_lock_tables();
  }

  bool finished = false;
  while (!finished) {
    if (!copy_once(false, &finished)) {
      return (false);
    }
  }

  /*
   * From this point forward, recv_parse_or_apply_log_rec_body should fail if
   * MLOG_INDEX_LOAD event is parsed and lock-ddl == false as its not safe to
   * continue the backup in any situation (with or without
   * --lock-ddl-per-table).
   */
  redo_catchup_completed = true;
  full_scan_tables_count = full_scan_tables.size();

  if (opt_lock_ddl == LOCK_DDL_ON) {
    ut_ad(reader.get_scanned_lsn() >= backup_redo_log_flushed_lsn);
  }

  debug_sync_point("xtrabackup_pause_after_redo_catchup");

  DBUG_EXECUTE_IF("xtrabackup_reopen_files_after_catchup",
                  const char *key = "log_files_find_and_analyze";
                  *const_cast<const char **>(&xtrabackup_debug_sync) = key;
                  force_reopen = true;);

  thread.start();

  error = false;

  return (true);
}

void Redo_Log_Data_Manager::track_archived_log(lsn_t start_lsn, const byte *buf,
                                               size_t len) {
  if (!archived_log_monitor.is_ready() ||
      archived_log_state == ARCHIVED_LOG_MATCHED) {
    return;
  }

  if (archived_log_state == ARCHIVED_LOG_NONE) {
    auto no = log_block_get_hdr_no(buf);
    if (no > archived_log_monitor.get_first_log_block_no()) {
      archived_log_monitor.skip_for_block(start_lsn, buf);
      archived_log_state = ARCHIVED_LOG_MATCHED;
    }
  }

  if (archived_log_state == ARCHIVED_LOG_NONE) {
    for (auto ptr = buf; ptr < buf + len;
         ptr += OS_FILE_LOG_BLOCK_SIZE, start_lsn += OS_FILE_LOG_BLOCK_SIZE) {
      auto no = log_block_get_hdr_no(ptr);
      auto checksum = log_block_get_checksum(ptr);
      if (no == archived_log_monitor.get_first_log_block_no() &&
          checksum == archived_log_monitor.get_first_log_block_checksum()) {
        archived_log_monitor.get_reader().set_start_lsn(start_lsn);
        archived_log_state = ARCHIVED_LOG_MATCHED;
      }
    }
  }

  if (archived_log_state == ARCHIVED_LOG_MATCHED) {
    if (archived_log_monitor.get_reader().seek_logfile(
            reader.get_contiguous_lsn())) {
      xb::info() << "Switched to archived redo log starting with LSN "
                 << reader.get_contiguous_lsn();
      archived_log_state = ARCHIVED_LOG_POSITIONED;
    } else {
      ib::warn() << "Failed to seek Archive log file from LSN " << start_lsn;
    }
  }
}

bool Redo_Log_Data_Manager::has_parsed_lsn(lsn_t lsn) const {
  /* check if we have parsed up to desired lsn, or if we have not parsed
   * anything (no redo) or are in the middle of last block */
  return (parser.get_last_parsed_lsn() >= lsn ||
          parser.get_last_parsed_lsn() == 0 ||
          lsn - parser.get_last_parsed_lsn() < OS_FILE_LOG_BLOCK_SIZE);
}

bool Redo_Log_Data_Manager::copy_once(bool is_last, bool *finished) {
  auto start_lsn = reader.get_contiguous_lsn();

  if (archived_log_state == ARCHIVED_LOG_POSITIONED) {
    auto &archive_reader = archived_log_monitor.get_reader();

    if (archive_reader.seek_logfile(start_lsn)) {
      auto len = archive_reader.read_logfile(finished);

      if (len < 0) {
        return (false);
      }

      if (len > 0) {
        if (!parser.parse_log(archive_reader.get_buffer(), len, start_lsn)) {
          return (false);
        }

        if (!writer.write_buffer(archive_reader.get_buffer(), len)) {
          return (false);
        }

        reader.seek_logfile(archive_reader.get_contiguous_lsn());
        scanned_lsn = archive_reader.get_contiguous_lsn();
        return (true);
      }

      if (stop_lsn == 0 || archive_reader.get_contiguous_lsn() >= stop_lsn) {
        return (true);
      }
    } else {
      ib::warn() << "Failed to seek Archive log file from LSN " << start_lsn
                 << " Will fallback to live redo ";
    }
  }

  auto len = reader.read_logfile(is_last, finished);
  error = reader.is_error();

  if (len <= 0) {
    return (len == 0);
  }

  track_archived_log(start_lsn, reader.get_buffer(), len);

  if (!parser.parse_log(reader.get_buffer(), len, start_lsn)) {
    return (false);
  }

  if (!writer.write_buffer(reader.get_buffer(), len)) {
    return (false);
  }
  scanned_lsn = reader.get_scanned_lsn();
  return (true);
}

void Redo_Log_Data_Manager::copy_func() {
  my_thread_init();
  /* create THD to get thread number in the error log */
  THD *thd = create_thd(false, false, true, 0, 0);

  aborted = false;
  if (xtrabackup_register_redo_log_consumer &&
      redo_log_consumer_can_advance.load()) {
    redo_log_consumer.advance(redo_log_consumer_cnx, reader.get_scanned_lsn());
  }

  bool finished;
  lsn_t consumer_lsn = 0;
  while (!aborted && (stop_lsn == 0 || stop_lsn > reader.get_scanned_lsn())) {
    xtrabackup_io_throttling();

    if (!copy_once(false, &finished)) {
      error = true;
      return;
    }

    if (xtrabackup_register_redo_log_consumer &&
        redo_log_consumer_can_advance.load()) {
      if (archived_log_monitor.is_ready() &&
          archived_log_state == ARCHIVED_LOG_POSITIONED) {
        redo_log_consumer.deinit(redo_log_consumer_cnx);
        mysql_close(redo_log_consumer_cnx);
        xtrabackup_register_redo_log_consumer = false;
      } else {
        if (consumer_lsn != reader.get_scanned_lsn()) {
          consumer_lsn = reader.get_scanned_lsn();
          redo_log_consumer.advance(redo_log_consumer_cnx, consumer_lsn);
        }
      }
    }

    if (finished) {
      xb::info() << ">> log scanned up to (" << reader.get_scanned_lsn() << ")";

      debug_sync_point("xtrabackup_copy_logfile_pause");

      os_event_reset(event);
      os_event_wait_time_low(event, std::chrono::milliseconds{copy_interval},
                             0);
    }
  }

  if (!aborted && !copy_once(true, &finished)) {
    error = true;
  }

  destroy_thd(thd);
  my_thread_end();
}

lsn_t Redo_Log_Data_Manager::get_last_checkpoint_lsn() const {
  return (last_checkpoint_lsn);
}

lsn_t Redo_Log_Data_Manager::get_stop_lsn() const { return (stop_lsn); }

lsn_t Redo_Log_Data_Manager::get_start_checkpoint_lsn() const {
  return (start_checkpoint_lsn);
}

lsn_t Redo_Log_Data_Manager::get_scanned_lsn() const { return (scanned_lsn); }

lsn_t Redo_Log_Data_Manager::get_parsed_lsn() const {
  return parser.get_last_parsed_lsn();
}

void Redo_Log_Data_Manager::set_copy_interval(ulint interval) {
  copy_interval = interval;
}

ulint Redo_Log_Data_Manager::get_copy_interval() const { return copy_interval; }

void Redo_Log_Data_Manager::abort() {
  aborted = true;
  os_event_set(event);
  thread.join();
  archived_log_monitor.stop();
}

bool Redo_Log_Data_Manager::is_error() const { return (error); }

Redo_Log_Data_Manager::~Redo_Log_Data_Manager() { os_event_destroy(event); }

bool Redo_Log_Data_Manager::stop_at(lsn_t lsn, lsn_t checkpoint_lsn) {
  last_checkpoint_lsn = checkpoint_lsn;
  xb::info() << "The latest check point (for incremental): "
             << SQUOTE(last_checkpoint_lsn);
  xb::info() << "Stopping log copying thread at LSN " << lsn;

  stop_lsn = lsn;
  os_event_set(event);
  thread.join();

  archived_log_monitor.stop();

  /* to ensure redo logs are not disabled during the backup, reopen the log
  files to read HEADER. */
  if (opt_lock_ddl != LOCK_DDL_ON && archived_log_state == ARCHIVED_LOG_NONE &&
      !reopen_log_files(0)) {
    xb::error() << "Failed to open redo log files. ";
    return (false);
  }
  log_crash_safe_validate(*log_sys);

  scanned_lsn = reader.get_scanned_lsn();

  if (last_checkpoint_lsn > scanned_lsn) {
    xb::error() << "last checkpoint LSN (" << last_checkpoint_lsn
                << ") is larger than last copied LSN (" << scanned_lsn << ").";

    return (false);
  }

  if (!writer.close_logfile()) {
    return (false);
  }

  if (xtrabackup_register_redo_log_consumer) {
    redo_log_consumer.deinit(redo_log_consumer_cnx);
    mysql_close(redo_log_consumer_cnx);
  }

  return (true);
}

void Redo_Log_Data_Manager::close() {
  log_sys_close();
  archived_log_monitor.close();
  os_event_destroy(event);
}
