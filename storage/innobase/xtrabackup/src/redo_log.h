/******************************************************
Copyright (c) 2019,2022 Percona LLC and/or its affiliates.

Data sink interface.

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

#ifndef XB_REDO_LOG_H
#define XB_REDO_LOG_H

#include <log0log.h>
#include <log0types.h>
#include <os0thread-create.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "datasink.h"
#include "redo_log_consumer.h"

#define redo_log_read_buffer_size ((srv_log_buffer_size) / 2)

/** Redo log reader. */
class Redo_Log_Reader {
 public:
  Redo_Log_Reader();

  /** Find start checkpoint lsn.
  @param[out] lsn               start checkpoint lsn
  @return true if success. */
  bool find_start_checkpoint_lsn();

  /** Get log header. */
  byte *get_header() const;

  /** Get log buffer. */
  byte *get_buffer() const;

  /** Get scanned LSN. */
  lsn_t get_scanned_lsn() const;

  /** Get contiguous LSN. */
  lsn_t get_contiguous_lsn() const;

  /** Get checkpoint LSN at the backup start. */
  lsn_t get_start_checkpoint_lsn() const;

  /** Read from logfile into internal buffer
  @param[in] is_last            true if this is last read
  @param[in] finished           true if there is no more data to read
                                for now
  @return read length or -1 if error. */
  ssize_t read_logfile(bool is_last, bool *finished);

  /** Seek logfile to specified lsn. */
  void seek_logfile(lsn_t lsn);

  /** Whether there was an error. */
  bool is_error() const;

 private:
  /** log header buffer. */
  ut::aligned_array_pointer<byte, UNIV_PAGE_SIZE_MAX> log_hdr_buf;

  /** log read buffer. */
  ut::aligned_array_pointer<byte, UNIV_PAGE_SIZE_MAX> log_buf;

  /** Read specified log segment into a buffer.
  @param[in,out] buf            buffer where to read
  @param[in]     start_lsn      read area start
  @param[in]     end_lsn        read area end
  @param[out]    read_upto_lsn  scanning succeeded up to this lsn
  @param[in]     checkpoint_lsn checkpoint lsn
  @param[in] finished           true if there is no more data to read
                                for now
  @return scanned length or -1 if error. */
  ssize_t scan_log_recs(byte *buf, bool is_last, lsn_t start_lsn,
                        lsn_t *read_upto_lsn, lsn_t checkpoint_lsn,
                        bool *finished);

  /** Read specified log segment into a buffer.
  @param[in,out] log            redo log
  @param[in,out] buf            buffer where to read
  @param[in]     start_lsn      read area start
  @param[in]     end_lsn        read area end
  @return lsn up to which data was available on disk (ideally end_lsn)
  */
  static lsn_t read_log_seg(log_t &log, byte *buf, lsn_t start_lsn,
                            const lsn_t end_lsn);

  /** checkpoint LSN at the backup start. */
  lsn_t checkpoint_lsn_start{0};

  /** last scanned LSN. */
  lsn_t log_scanned_lsn{0};

  /** error flag. */
  static std::atomic<bool> m_error;
};

/** Redo log parser. */
class Redo_Log_Parser {
 public:
  /** Parse log from given buffer.
  @param[in] buf                buffer to parse
  @param[in] len                data length
  @param[in] start_lsn          start lsn
  @return false if error. */
  bool parse_log(const byte *buf, size_t len, lsn_t start_lsn);

  /** Get last parsed LSN. */
  lsn_t get_last_parsed_lsn() const;

 private:
  /** last parsed LSN */
  std::atomic<lsn_t> last_parsed_lsn{0};
};

/** Redo log writer. */
class Redo_Log_Writer {
 public:
  Redo_Log_Writer();

  /** Create logfile with given path.
  @param[in] path               log file name and path
  @return false if error. */
  bool create_logfile(const char *path);

  /** Write log header.
  @param[in] buf                buffer where to write from
  @return false if error. */
  bool write_header(byte *hdr);

  /** Write buffer contents into logfile.
  @param[in] buf                buffer where to write from
  @param[in] len                data length
  @return false if error. */
  bool write_buffer(byte *buf, size_t len);

  /** Close logfile.
  @return false if error. */
  bool close_logfile();

 private:
  /** Log file. */
  ds_file_t *log_file;

  /** Temporary buffer used for encryption. */
  ut::aligned_array_pointer<byte, UNIV_PAGE_SIZE_MAX> scratch_buf;
};

/** Archived redo log reader. */
class Archived_Redo_Log_Reader {
 public:
  Archived_Redo_Log_Reader();

  /** Set file descriptor of the archived log file. */
  void set_fd(File fd);

  /** Read from logfile into internal buffer
  @param[in] finished           true if there is no more data to read
                                for now
  @return read length or -1 if error. */
  ssize_t read_logfile(bool *finished);

  /** Seek logfile to specified lsn.
  @param[in] lsn                desired lsn
  @return true if success */
  bool seek_logfile(lsn_t lsn);

  /** Set start lsn of archived log. */
  void set_start_lsn(lsn_t lsn);

  lsn_t get_start_lsn() const { return archive_start_lsn; }

  /** Get log buffer. */
  byte *get_buffer() const;

  /** Get contiguous LSN. */
  lsn_t get_contiguous_lsn() const;

 private:
  /** log file. */
  File file;

  /** log read buffer. */
  ut::aligned_array_pointer<byte, UNIV_PAGE_SIZE_MAX> log_buf;

  /** temporary buffer used for encryption. */
  ut::aligned_array_pointer<byte, UNIV_PAGE_SIZE_MAX> scratch_buf;

  /** start lsn of archived redo log. */
  lsn_t archive_start_lsn;

  /** last scanned LSN. */
  lsn_t log_scanned_lsn;
};

/** Archived redo monitor. */
class Archived_Redo_Log_Monitor {
 public:
  Archived_Redo_Log_Monitor();
  ~Archived_Redo_Log_Monitor();

  /** Start log archiving monitor. */
  void start();

  /** Signal monitor to stop. */
  void stop();

  /** Release used resources. */
  void close();

  /** Get archived log reader. */
  Archived_Redo_Log_Reader &get_reader();

  /** Whether the archived log is created and readable. */
  bool is_ready() const;

  /** Get first log block no from the archived redo log. */
  uint32_t get_first_log_block_no() const;

  /** Get first log block checksum from the archived redo log. */
  uint32_t get_first_log_block_checksum() const;

  /** Read archived log until the given log block. */
  void skip_for_block(lsn_t lsn, const byte *redo_buf);

 private:
  /** Parse the value of innodb_redo_log_archive_dirs. */
  void parse_archive_dirs(const std::string &s);

  /** In case of error return the configuration back to the original value  */
  void archive_error_handle(MYSQL *mysql);

  /** Start log archiving on server if supported, open archive file,
      wait for stop signal and remove the archive. */
  void thread_func();

  /** parsed value of innodb_redo_log_archive_dirs. */
  std::unordered_map<std::string, std::string> archived_dirs;

  /** monitor thread. */
  IB_thread thread;

  /** stopped flag. */
  std::atomic<bool> stopped;

  /** readiness flag. */
  std::atomic<bool> ready;

  /** controls if xtrabackup has set redo log arch. Can only happen if it was
      set to null */
  std::atomic<bool> xb_has_set_redo_log_arch;

  /** first log block no. */
  uint32_t first_log_block_no;

  /** first log block checksum. */
  uint32_t first_log_block_checksum;

  /** archived redo log reader. */
  Archived_Redo_Log_Reader reader;

  /** stop event. */
  os_event_t event;
};

class Redo_Log_Data_Manager {
 public:
  /** Init data manager. */
  bool init();

  /** Start log copying. */
  bool start();

  /** Abort log copying. */
  void abort();

  /** Stop log copying at specific lsn. */
  bool stop_at(lsn_t lsn, lsn_t checkpoint_lsn);

  /** Close log file. */
  void close();

  /** Get checkpoint lsn at the end of the backup. */
  lsn_t get_last_checkpoint_lsn() const;

  /** Get checkpoint lsn at the beginning of the backup. */
  lsn_t get_start_checkpoint_lsn() const;

  /** Get backup stop lsn (consistency point). */
  lsn_t get_stop_lsn() const;

  /** Get last scanned lsn. */
  lsn_t get_scanned_lsn() const;

  /** Get last parsed lsn. */
  lsn_t get_parsed_lsn() const;

  /** Get copy interval. */
  ulint get_copy_interval() const;

  /** Set copy interval. */
  void set_copy_interval(ulint interval);

  /** Whether there was an error. */
  bool is_error() const;

  /** whether we have parsed up to LSN */
  bool has_parsed_lsn(lsn_t lsn) const;

  ~Redo_Log_Data_Manager();

 private:
  /** Log copying func. */
  void copy_func();

  /** Copy batch of log blocks. */
  bool copy_once(bool is_last, bool *finished);

  /** Compare archived log block number and lsn with the current lsn
      and seek archived log if needed. */
  void track_archived_log(lsn_t start_lsn, const byte *buf, size_t len);

  /** copying thread. */
  IB_thread thread;

  /** aborted flag. */
  std::atomic<bool> aborted;

  /** lsn to stop copying at. */
  std::atomic<lsn_t> stop_lsn;

  /** checkpoint lsn at the end of the backup. */
  lsn_t last_checkpoint_lsn;

  /** checkpoint lsn at the beginning of the backup. */
  lsn_t start_checkpoint_lsn;

  /** last scanned lsn. */
  lsn_t scanned_lsn;

  /** redo log reader. */
  Redo_Log_Reader reader;

  /** redo log writer. */
  Redo_Log_Writer writer;

  /** redo log parser. */
  Redo_Log_Parser parser;

  /** archived log monitor. */
  Archived_Redo_Log_Monitor archived_log_monitor;

  /** time to sleep in ms between log copying batches. */
  ulint copy_interval;

  /** stop event. */
  os_event_t event;

  /** error flag. */
  std::atomic<bool> error;

  /** redo log consumer */
  Redo_Log_Consumer redo_log_consumer;

  /** MySQL connection to register redo log consumer */
  MYSQL *redo_log_consumer_cnx = nullptr;

  enum {
    ARCHIVED_LOG_NONE,
    ARCHIVED_LOG_MATCHED,
    ARCHIVED_LOG_POSITIONED
  } archived_log_state;
};

#endif
