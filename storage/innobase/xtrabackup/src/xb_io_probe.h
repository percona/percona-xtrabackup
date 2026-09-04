/******************************************************
Copyright (c) 2026 Percona LLC and/or its affiliates.

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

/* Storage probe for --page-tracking-merge-gap=auto (PXB-3862).

Measures the two device characteristics the read request cost needs
(see read_request_cost_bytes() below): the sequential read bandwidth,
from a few large contiguous reads, then the per-request round trip,
from single-page reads scattered across the file. ~28 reads / ~16MB
total, well under a second on any storage.

Plain POSIX on purpose: no server I/O layer, no xtrabackup globals, so
the probe is unit-testable standalone and can be pointed at any file -
including a live mysqld datadir - by the env-gated gunit case in
unittest/gunit/innodb/xb_page_group-t.cc. */

#ifndef XB_IO_PROBE_H
#define XB_IO_PROBE_H

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace pagetracking {

/** Files smaller than this cannot be measured meaningfully (the
sequential samples would not reach steady state); the probe rejects
them and the caller keeps the fallback read request cost. */
constexpr uint64_t PROBE_MIN_FILE_BYTES = 64 * 1024 * 1024;

/** Result of probe_storage(). */
struct Probe_result {
  uint64_t rtt_us{0};           /*!< per-request round trip, median */
  uint64_t bw_bytes_per_sec{0}; /*!< sequential read bandwidth */
};

/** Drop a byte range of the file from the OS page cache, so the probe
times the device rather than RAM (the range may be warm from the server
or a previous scan). Newer kernels cache a sequentially read file in
blocks of up to 2MB and ignore the advice unless it covers a whole
block, so the range is widened to full 2MB multiples. The advice skips
dirty pages; under O_DIRECT it is a no-op.

@param[in] fd      file the probe reads from
@param[in] offset  start of the byte range about to be read
@param[in] length  length of that range */
inline void evict_from_page_cache([[maybe_unused]] int fd,
                                  [[maybe_unused]] uint64_t offset,
                                  [[maybe_unused]] uint64_t length) {
#ifdef POSIX_FADV_DONTNEED
  constexpr uint64_t EVICT_SIZE_BYTES = 2 * 1024 * 1024;
  /* the file is a row of 2MB units; dividing a byte position by the
  unit size gives the number of the unit it falls in. Evict every unit
  the range touches, from the one holding its first byte to the one
  holding its last byte, in full. */
  const uint64_t first_unit = offset / EVICT_SIZE_BYTES;
  const uint64_t last_unit = (offset + length - 1) / EVICT_SIZE_BYTES;
  ::posix_fadvise(fd, first_unit * EVICT_SIZE_BYTES,
                  (last_unit - first_unit + 1) * EVICT_SIZE_BYTES,
                  POSIX_FADV_DONTNEED);
#endif
}

/** Measure the storage behind a file.

Every read below fetches whole 16KB pages at page-aligned offsets - the
same shape as the copy loop's reads, and aligned as O_DIRECT requires.
One file descriptor serves both phases: pread carries no file position,
and closing/reopening between them would evict nothing anyway (the OS
page cache is keyed by inode, and the device's internal cache is beyond
user space either way).

The sequential phase runs FIRST, on pages nothing has touched yet, so
its bandwidth cannot be inflated by caching; any cross-phase cache
effect can then only make the later scattered reads faster, and
understating the round trip shrinks the resulting cost - the safe
direction.

@param[in] path          file to probe (a large data file of the backup
                         source, so the numbers describe the same device
                         the copy will read)
@param[in] use_o_direct  open with O_DIRECT, matching how the backup
                         will read the data files; without it, page
                         cache hits can understate the round trip (the
                         resulting cost is then smaller - the safe
                         direction)
@return measured characteristics, or std::nullopt when the file cannot
        be opened, is smaller than PROBE_MIN_FILE_BYTES, or a read
        fails */
inline std::optional<Probe_result> probe_storage(const char *path,
                                                 bool use_o_direct) {
  using clock = std::chrono::steady_clock;
  using std::chrono::duration_cast;
  using std::chrono::microseconds;

  constexpr uint64_t PROBE_PAGE_BYTES = 16 * 1024;
  constexpr int RTT_SAMPLES = 12;
  constexpr uint64_t SEQ_CHUNK_BYTES = 4 * 1024 * 1024;
  constexpr int SEQ_SAMPLES = 4;
  constexpr size_t BUF_ALIGN = 4096; /* covers any O_DIRECT block size */

  int flags = O_RDONLY;
#ifdef O_DIRECT
  if (use_o_direct) {
    flags |= O_DIRECT;
  }
#endif

  const int fd = ::open(path, flags);
  if (fd < 0) {
    return (std::nullopt);
  }

  struct stat file_stat;
  if (::fstat(fd, &file_stat) != 0 ||
      static_cast<uint64_t>(file_stat.st_size) < PROBE_MIN_FILE_BYTES) {
    ::close(fd);
    return (std::nullopt);
  }
  const uint64_t file_pages =
      static_cast<uint64_t>(file_stat.st_size) / PROBE_PAGE_BYTES;

  void *buf = nullptr;
  if (::posix_memalign(&buf, BUF_ALIGN, SEQ_CHUNK_BYTES) != 0) {
    ::close(fd);
    return (std::nullopt);
  }

  /* contiguous pages from the middle of the file onward -> bandwidth
  (PROBE_MIN_FILE_BYTES guarantees the chunks fit after the midpoint) */
  Probe_result result;
  evict_from_page_cache(fd, (file_pages / 2) * PROBE_PAGE_BYTES,
                        SEQ_SAMPLES * SEQ_CHUNK_BYTES);
  uint64_t seq_page_no = file_pages / 2;
  uint64_t seq_bytes = 0;
  const auto seq_start = clock::now();
  for (int i = 0; i < SEQ_SAMPLES; i++) {
    const ssize_t n_read =
        ::pread(fd, buf, SEQ_CHUNK_BYTES, seq_page_no * PROBE_PAGE_BYTES);
    if (n_read <= 0) {
      break;
    }
    seq_bytes += n_read;
    seq_page_no += n_read / PROBE_PAGE_BYTES;
  }
  /* the max() makes the division below total; sub-microsecond timing
  of a 16MB read cannot happen on real hardware */
  const uint64_t seq_us = std::max<uint64_t>(
      duration_cast<microseconds>(clock::now() - seq_start).count(), 1);
  if (seq_bytes == 0) {
    ::free(buf);
    ::close(fd);
    return (std::nullopt);
  }
  result.bw_bytes_per_sec = seq_bytes * 1000000 / seq_us;

  /* one page read at every 1/13th of the file -> round trip: each read
  fetches page (file_pages / 13) * i, mirroring how the copy loop
  fetches one isolated changed page. The median is used so a stray
  stall or cache hit cannot skew the estimate. */
  std::vector<uint64_t> rtt_samples;
  for (int i = 1; i <= RTT_SAMPLES; i++) {
    const uint64_t page_no = (file_pages / (RTT_SAMPLES + 1)) * i;
    evict_from_page_cache(fd, page_no * PROBE_PAGE_BYTES, PROBE_PAGE_BYTES);
    const auto start = clock::now();
    if (::pread(fd, buf, PROBE_PAGE_BYTES, page_no * PROBE_PAGE_BYTES) !=
        static_cast<ssize_t>(PROBE_PAGE_BYTES)) {
      ::free(buf);
      ::close(fd);
      return (std::nullopt);
    }
    rtt_samples.push_back(
        duration_cast<microseconds>(clock::now() - start).count());
  }
  std::nth_element(rtt_samples.begin(),
                   rtt_samples.begin() + rtt_samples.size() / 2,
                   rtt_samples.end());
  /* a cache-served page read takes under a microsecond and truncates to
  0; floor it to 1 so the round trip stays a meaningful, nonzero value
  (nothing divides by it - understating it only shrinks the cost, the
  safe direction) */
  result.rtt_us = std::max<uint64_t>(rtt_samples[rtt_samples.size() / 2], 1);

  ::free(buf);
  ::close(fd);
  return (result);
}

/** clamp bounds for the measured read request cost */
constexpr uint64_t READ_REQUEST_COST_MIN_BYTES = 64 * 1024;
constexpr uint64_t READ_REQUEST_COST_MAX_BYTES = 1024 * 1024;

/** Fallback read request cost when the storage could not be probed: a
conservative cut across common storage classes. */
constexpr uint64_t READ_REQUEST_COST_FALLBACK_BYTES = 512 * 1024;

/** Safety margin dividing the raw break-even (rtt * bandwidth).

A margin is needed because the probe measures raw device bandwidth,
while merged filler bytes also flow through the copy pipeline
(checksum, buffer management) whose effective bandwidth is lower, and
because the risk is asymmetric: too large a cost makes backups slower
than unmerged reads (a regression), too small only misses part of the
win.

1.5 was calibrated on two instrumented machines whose measured
break-evens bound it from both sides: a shared-disk NVMe (raw 171KB,
true break-even ~131KB: merging across 144KB gaps measurably loses)
needs a margin >= ~1.3, while a split-data-and-backup-disk server
(raw ~245KB, effective ~= raw: merging across 144KB gaps measurably
wins) needs the cost to stay above 144KB, i.e. a margin <= ~1.7.
1.5 satisfies both with headroom; 2 was over-conservative and refused
profitable merges on the split-disk machine. */
constexpr double READ_REQUEST_COST_MARGIN = 1.5;

/** Compute the read request cost: what one read request costs the
backup, expressed in bytes of sequential transfer. It bounds gap
merging - the most bytes worth reading across one gap of unchanged
pages to save one read request - and is derived from measured storage
characteristics.

Merging across a gap saves one request round trip (rtt) and costs
gap_bytes / bandwidth of transfer, so the raw break-even is
rtt * bandwidth, reduced by READ_REQUEST_COST_MARGIN (see there).

Reference points: local NVMe 0.15ms x 1.2GB/s -> ~115KB (refuses the
merges that regress there); split-disk server 0.165ms x 1.5GB/s ->
~165KB (merges the 144KB gaps that win there); ~1ms cloud volume x
400MB/s -> ~250KB; HDD 8ms x 150MB/s -> ~800KB.

@param[in] rtt_us            measured per-request round trip, microseconds
@param[in] bw_bytes_per_sec  measured sequential read bandwidth
@return read request cost in bytes, clamped to
        [READ_REQUEST_COST_MIN_BYTES, READ_REQUEST_COST_MAX_BYTES] */
inline uint64_t read_request_cost_bytes(uint64_t rtt_us,
                                        uint64_t bw_bytes_per_sec) {
  const double rtt_seconds = static_cast<double>(rtt_us) / 1000000.0;
  /* the bytes one round trip's worth of sequential transfer moves,
  reduced by the safety margin */
  const uint64_t raw =
      static_cast<uint64_t>(static_cast<double>(bw_bytes_per_sec) *
                            rtt_seconds / READ_REQUEST_COST_MARGIN);
  if (raw < READ_REQUEST_COST_MIN_BYTES) {
    return (READ_REQUEST_COST_MIN_BYTES);
  }
  if (raw > READ_REQUEST_COST_MAX_BYTES) {
    return (READ_REQUEST_COST_MAX_BYTES);
  }
  return (raw);
}

}  // namespace pagetracking

#endif /* XB_IO_PROBE_H */
