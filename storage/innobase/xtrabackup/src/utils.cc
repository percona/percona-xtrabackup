/******************************************************
Copyright (c) 2021 Percona LLC and/or its affiliates.

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
#include <my_alloc.h>
#include <my_default.h>
#include <mysqld.h>

#ifdef __APPLE__
#include <mach/mach_host.h>
#include <sys/sysctl.h>
#else
#ifdef HAVE_PROCPS_V3
#include <proc/sysinfo.h>
#else
#include <libproc2/meminfo.h>
#endif                                     // HAVE_PROCPS_V3
#endif                                     // __APPLE__
#include <boost/uuid/uuid.hpp>             // uuid class
#include <boost/uuid/uuid_generators.hpp>  // generators
#include <boost/uuid/uuid_io.hpp>          // streaming operators etc.
#include <sstream>
#include "common.h"
#include "msg.h"
#include "xtrabackup.h"

static boost::uuids::random_generator gen = boost::uuids::random_generator();

namespace xtrabackup {
namespace utils {

bool load_backup_my_cnf(my_option *options, char *path) {
  static MEM_ROOT argv_alloc{PSI_NOT_INSTRUMENTED, 512};
  const char *groups[] = {"mysqld", NULL};

  char *exename = (char *)"xtrabackup";
  char **backup_my_argv = &exename;
  int backup_my_argc = 1;
  char config_file[FN_REFLEN];

  /* we need full name so that only backup-my.cnf will be read */
  if (fn_format(config_file, "backup-my.cnf", path, "",
                MY_UNPACK_FILENAME | MY_SAFE_PATH) == NULL) {
    return (false);
  }

  if (my_load_defaults(config_file, groups, &backup_my_argc, &backup_my_argv,
                       &argv_alloc, NULL)) {
    return (false);
  }

  if (handle_options(&backup_my_argc, &backup_my_argv, options, NULL)) {
    return (false);
  }

  return (true);
}

bool read_server_uuid() {
  /* for --stats we not always have a backup-my.cnf */
  if (xtrabackup_stats) return true;

  char *uuid = NULL;
  bool ret;
  my_option config_options[] = {
      {"server-uuid", 0, "", &uuid, &uuid, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0,
       0, 0},
      {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};
  if (xtrabackup_incremental_dir != nullptr) {
    ret = xtrabackup::utils::load_backup_my_cnf(config_options,
                                                xtrabackup_incremental_dir);
  } else {
    ret = xtrabackup::utils::load_backup_my_cnf(config_options,
                                                xtrabackup_real_target_dir);
  }
  if (!ret) {
    msg("xtrabackup: Error: failed to load backup-my.cnf\n");
    return (false);
  }
  memset(server_uuid, 0, Encryption::SERVER_UUID_LEN + 1);
  if (uuid != NULL) {
    strncpy(server_uuid, uuid, Encryption::SERVER_UUID_LEN);
  }
  return (true);
}

/* find the pxb base version */
unsigned long get_version_number(std::string version_str) {
  unsigned long major = 0, minor = 0, version = 0;
  std::size_t major_p = version_str.find(".");
  if (major_p != std::string::npos)
    major = stoi(version_str.substr(0, major_p));

  std::size_t minor_p = version_str.find(".", major_p + 1);
  if (minor_p != std::string::npos)
    minor = stoi(version_str.substr(major_p + 1, minor_p - major_p));

  std::size_t version_p = version_str.find(".", minor_p + 1);
  if (version_p != std::string::npos)
    version = stoi(version_str.substr(minor_p + 1, version_p - minor_p));
  else
    version = stoi(version_str.substr(minor_p + 1));
  return major * 10000 + minor * 100 + version;
}

#ifdef __APPLE__
unsigned long host_total_memory() {
  unsigned long total_mem = sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGESIZE);
  return total_mem;
}

unsigned long host_free_memory() {
  unsigned long total_mem = host_total_memory();
  int64_t used_mem;
  vm_size_t page_size;
  mach_msg_type_number_t count;
  vm_statistics_data_t vm_stats;

  // Get used memory
  mach_port_t host = mach_host_self();
  count = sizeof(vm_stats) / sizeof(natural_t);
  if (KERN_SUCCESS == host_page_size(host, &page_size) &&
      KERN_SUCCESS ==
          host_statistics(host, HOST_VM_INFO, (host_info_t)&vm_stats, &count)) {
    used_mem = ((int64_t)vm_stats.active_count + (int64_t)vm_stats.wire_count) *
               (int64_t)page_size;

    ut_a(total_mem >= (unsigned long)used_mem);
    return total_mem - (unsigned long)used_mem;
  }
  return 0;
}
#else
unsigned long host_total_memory() {
#ifdef HAVE_PROCPS_V3
  meminfo();
  return kb_main_total * 1024;
#else
  struct meminfo_info *mem_info;
  if (procps_meminfo_new(&mem_info) < 0) {
    return 0;
  }

  return MEMINFO_GET(mem_info, MEMINFO_MEM_TOTAL, ul_int) * 1024;
#endif  // HAVE_PROCPS_V3
}

unsigned long host_free_memory() {
#ifdef HAVE_PROCPS_V3
  meminfo();
  return kb_main_available * 1024;
#else
  struct meminfo_info *mem_info;
  if (procps_meminfo_new(&mem_info) < 0) {
    return 0;
  }

  return MEMINFO_GET(mem_info, MEMINFO_MEM_AVAILABLE, ul_int) * 1024;
#endif  // HAVE_PROCPS_V3
}
#endif

std::string generate_uuid() {
  boost::uuids::uuid uuid = gen();
  std::ostringstream uuid_ss;
  uuid_ss << uuid;
  return uuid_ss.str();
}

}  // namespace utils
}  // namespace xtrabackup
