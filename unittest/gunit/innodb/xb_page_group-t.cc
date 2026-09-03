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

/* Tests for the --page-tracking read-range grouping (PXB-3862).
See storage/innobase/xtrabackup/src/xb_page_group.h. */

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "storage/innobase/xtrabackup/src/xb_io_probe.h"

/* Tests for the PXB-3862 read request cost model and storage probe.
The grouping itself is one comparison applied by the read path
(range_get_next_page) and is covered by the framework testcase
suites/pagetracking/xb_pagetracking_merge_gap.sh. */

namespace xb_page_group_test {

using pagetracking::read_request_cost_bytes;
using pagetracking::READ_REQUEST_COST_MAX_BYTES;
using pagetracking::READ_REQUEST_COST_MIN_BYTES;

TEST(XbReadRequestCost, FromMeasurements) {
  /* rtt * bw * 2/3: NVMe, split-disk server, cloud, HDD reference
  points (divisor calibrated on two instrumented machines; see
  read_request_cost_bytes) */
  EXPECT_EQ(146666u, read_request_cost_bytes(100, 2200000000ull));
  EXPECT_EQ(163350u, read_request_cost_bytes(165, 1485000000ull));
  EXPECT_EQ(253333u, read_request_cost_bytes(950, 400000000ull));
  EXPECT_EQ(800000u, read_request_cost_bytes(8000, 150000000ull));
}

TEST(XbReadRequestCost, Clamps) {
  /* cached-read latencies cannot zero the cost, slow devices cannot
  blow it up */
  EXPECT_EQ(READ_REQUEST_COST_MIN_BYTES,
            read_request_cost_bytes(10, 1000000000ull));
  EXPECT_EQ(READ_REQUEST_COST_MAX_BYTES,
            read_request_cost_bytes(20000, 500000000ull));
}

TEST(XbReadRequestCost, DeviceContract) {
  /* the 10%-changed scenario: gaps of 9 pages = 144KB per gap.
  A fast shared-disk NVMe cost must sit below it (combining measured
  as a regression there); a split-disk server cost must sit above it
  (bridging measured as a win there). Same data, different storage. */
  EXPECT_LT(read_request_cost_bytes(120, 1200000000ull), 144u * 1024);
  EXPECT_GT(read_request_cost_bytes(165, 1485000000ull), 144u * 1024);
}

TEST(XbIoProbe, MissingFileIsRejected) {
  const auto result =
      pagetracking::probe_storage("/nonexistent/xb_io_probe_test", false);
  EXPECT_FALSE(result.has_value());
}

TEST(XbIoProbe, SmallFileIsRejected) {
  /* files below PROBE_MIN_FILE_BYTES cannot be measured meaningfully;
  the caller must keep the fallback read request cost */
  char path[] = "/tmp/xb_io_probe_small_XXXXXX";
  const int fd = mkstemp(path);
  ASSERT_GE(fd, 0);
  std::vector<char> content(1024 * 1024, 'x');
  ASSERT_EQ(static_cast<ssize_t>(content.size()),
            write(fd, content.data(), content.size()));
  close(fd);

  const auto result = pagetracking::probe_storage(path, false);
  EXPECT_FALSE(result.has_value());
  unlink(path);
}

/* Manual mode: point XB_PROBE_FILE at any large data file - e.g. a
tablespace of a live mysqld datadir - to run the real probe and print
the measured characteristics and the resulting read request cost:

  XB_PROBE_FILE=~/sandboxes/msb_8_4/data/test/sbtest1.ibd \
  XB_PROBE_O_DIRECT=1 ./xb_page_group-t --gtest_filter='XbIoProbe.*'
*/
TEST(XbIoProbe, MeasuresProvidedDatadirFile) {
  const char *path = getenv("XB_PROBE_FILE");
  if (path == nullptr) {
    GTEST_SKIP() << "set XB_PROBE_FILE=<data file >= 64MB> to run";
  }
  const bool use_o_direct = getenv("XB_PROBE_O_DIRECT") != nullptr;

  const auto result = pagetracking::probe_storage(path, use_o_direct);
  ASSERT_TRUE(result.has_value()) << "could not probe " << path;
  EXPECT_GT(result->rtt_us, 0u);
  EXPECT_GT(result->bw_bytes_per_sec, 0u);

  const auto cost =
      read_request_cost_bytes(result->rtt_us, result->bw_bytes_per_sec);
  std::cout << "probe of " << path << (use_o_direct ? " (O_DIRECT)" : "")
            << ": request round trip " << result->rtt_us
            << " us, sequential read "
            << result->bw_bytes_per_sec / (1024 * 1024)
            << " MB/s -> one read request costs ~" << cost / 1024
            << "KB of sequential transfer\n";
}

}  // namespace xb_page_group_test
