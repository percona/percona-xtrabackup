/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <thread>

#include "mysql/harness/filesystem.h"
#include "router_component_test.h"

#include "dim.h"
#include "filesystem_utils.h"
#include "mock_server_testutils.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/utils.h"  // copy_file
#include "random_generator.h"
#include "router_component_testutils.h"

using namespace std::chrono_literals;
using namespace std::string_literals;

void RouterComponentTest::SetUp() {
  static mysql_harness::RandomGenerator static_rg;

  mysql_harness::DIM::instance().set_static_RandomGenerator(&static_rg);
}

void RouterComponentTest::TearDown() {
  shutdown_all();  // shutdown all that are still running.
  wait_for_exit();

  terminate_all_still_alive();  // terminate hanging processes.
  ensure_clean_exit();

  if (::testing::Test::HasFailure()) {
    dump_all();
  }
}

void RouterComponentTest::prepare_config_dir_with_default_certs(
    const std::string &config_dir) {
  mysql_harness::Path dst_dir(config_dir);
  auto datadir = dst_dir.join("data");

  mysql_harness::mkdir(datadir.str(), 0700, true);

  copy_default_certs_to_datadir(datadir.str());
}

void RouterComponentTest::copy_default_certs_to_datadir(
    const std::string &dst_dir) {
  mysql_harness::Path to(dst_dir);
  mysql_harness::Path from(SSL_TEST_DATA_DIR);

  mysqlrouter::copy_file(from.join("server-key.pem").str(),
                         to.join("router-key.pem").str());
  mysqlrouter::copy_file(from.join("server-cert.pem").str(),
                         to.join("router-cert.pem").str());
}

void RouterComponentTest::sleep_for(std::chrono::milliseconds duration) {
  if (getenv("WITH_VALGRIND")) {
    duration *= 10;
  }
  std::this_thread::sleep_for(duration);
}

bool RouterComponentTest::wait_log_contains(const ProcessWrapper &router,
                                            const std::string &pattern,
                                            std::chrono::milliseconds timeout) {
  if (getenv("WITH_VALGRIND")) {
    timeout *= 10;
  }

  const auto MSEC_STEP = 50ms;
  bool found = false;
  using clock_type = std::chrono::steady_clock;
  const auto end = clock_type::now() + timeout;
  do {
    const std::string log_content = router.get_logfile_content();
    found = pattern_found(log_content, pattern);
    if (!found) {
      auto step = std::min(timeout, MSEC_STEP);
      RouterComponentTest::sleep_for(step);
    }
  } while (!found && clock_type::now() < end);

  return found;
}

void RouterComponentTest::check_log_contains(
    const ProcessWrapper &router, const std::string &expected_string,
    size_t expected_occurences /*=1*/) {
  const std::string log_content = router.get_logfile_content();
  ASSERT_EQ(expected_occurences,
            count_str_occurences(log_content, expected_string))
      << "Expected: " << expected_string << "\n"
      << "in:\n"
      << log_content;
}

std::string RouterComponentTest::plugin_output_directory() {
  const auto bindir = get_origin().real_path();

  // if this is a multi-config-build, remember the build-type.
  auto build_type = bindir.basename().str();
  if (build_type == "runtime_output_directory") {
    // no multi-config build.
    build_type = {};
  }

  auto builddir = bindir.dirname();
  if (!build_type.empty()) {
    builddir = builddir.dirname();
  }
  auto plugindir = builddir.join("plugin_output_directory");
  if (!build_type.empty()) {
    plugindir = plugindir.join(build_type);
  }

  return plugindir.str();
}

stdx::expected<std::string, int>
RouterComponentTest::create_openid_connect_id_token_file(
    const std::string &subject, const std::string &identity_provider_name,
    int expiry, const std::string &private_key_file,
    const std::string &outdir) {
  auto jwt_exit_code =
      spawner(
          ProcessManager::get_origin().join("mysql_test_jwt_generator").str())
          .wait_for_sync_point(Spawner::SyncPoint::NONE)
          .spawn({
              subject,
              identity_provider_name,
              std::to_string(expiry),
              private_key_file,
              outdir,
          })
          .wait_for_exit();

  if (jwt_exit_code != 0) return stdx::unexpected(jwt_exit_code);

  return Path(outdir).join("jwt.txt").str();
}

constexpr const char RouterComponentBootstrapTest::kRootPassword[];

const RouterComponentBootstrapTest::OutputResponder
    RouterComponentBootstrapTest::kBootstrapOutputResponder{
        [](const std::string &line) -> std::string {
          if (line == "Please enter MySQL password for root: ")
            return kRootPassword + "\n"s;

          return "";
        }};

/**
 * the tiny power function that does all the work.
 *
 * - build environment
 * - start mock servers based on Config[]
 * - pass router_options to the launched router
 * - check the router exits as expected
 * - check output of router contains the expected lines
 */
void RouterComponentBootstrapTest::bootstrap_failover(
    const std::vector<Config> &mock_server_configs,
    const mysqlrouter::ClusterType cluster_type,
    const std::vector<std::string> &router_options, int expected_exitcode,
    const std::vector<std::string> &expected_output_regex,
    std::chrono::milliseconds wait_for_exit_timeout,
    const mysqlrouter::MetadataSchemaVersion &metadata_version,
    const std::vector<std::string> &extra_router_options) {
  std::string cluster_name("mycluster");

  std::vector<uint16_t> classic_ports;
  for (const auto &mock_server_config : mock_server_configs) {
    classic_ports.emplace_back(mock_server_config.port);
  }

  const auto cluster_nodes = classic_ports_to_cluster_nodes(classic_ports);
  const auto gr_nodes = classic_ports_to_gr_nodes(classic_ports);

  std::vector<std::tuple<ProcessWrapper &, unsigned int>> mock_servers;

  // shutdown the mock-servers when bootstrap is done.
  Scope_guard guard{[&mock_servers]() {
    // send a shutdown to all mocks
    for (auto [proc, port] : mock_servers) {
      proc.send_clean_shutdown_event();
    }

    // ... then wait for them all to finish.
    for (auto [proc, port] : mock_servers) {
      proc.wait_for_exit();
    }
  }};

  // start the mocks
  for (const auto &mock_server_config : mock_server_configs) {
    if (mock_server_config.js_filename.empty()) continue;

    // 0x10000 & 0xffff = 0 (port 0), but we bypass
    // libmysqlclient's default-port assignment
    const auto port =
        mock_server_config.unaccessible ? 0x10000 : mock_server_config.port;
    const auto http_port = mock_server_config.http_port;

    mock_servers.emplace_back(
        mock_server_spawner().spawn(
            mock_server_cmdline(mock_server_config.js_filename)
                .port(port)
                .http_port(http_port)
                .args()),
        port);

    set_mock_metadata(http_port, mock_server_config.cluster_specific_id,
                      gr_nodes, 0, cluster_nodes, 0, false, "127.0.0.1", "",
                      metadata_version, cluster_name);
  }

  std::vector<std::string> router_cmdline;

  if (!router_options.empty()) {
    router_cmdline = router_options;
  } else {
    router_cmdline.emplace_back("--bootstrap=127.0.0.1:" +
                                std::to_string(cluster_nodes[0].classic_port));

    router_cmdline.emplace_back("--connect-timeout");
    router_cmdline.emplace_back("1");
    router_cmdline.emplace_back("-d");
    router_cmdline.emplace_back(bootstrap_dir.name());
  }

  for (const auto &opt : extra_router_options) {
    router_cmdline.push_back(opt);
  }

  // launch the router
  auto &router =
      launch_router_for_bootstrap(router_cmdline, expected_exitcode, true);

  ASSERT_NO_FATAL_FAILURE(
      check_exit_code(router, expected_exitcode, wait_for_exit_timeout));

  // split the output into lines
  std::vector<std::string> lines;
  {
    std::istringstream ss{router.get_full_output()};

    for (std::string line; std::getline(ss, line);) {
      lines.emplace_back(line);
    }
  }

  for (auto const &re_str : expected_output_regex) {
    EXPECT_THAT(lines, ::testing::Contains(::testing::ContainsRegex(re_str)))
        << mock_servers;
  }

  if (EXIT_SUCCESS == expected_exitcode) {
    const std::string cluster_type_name =
        cluster_type == mysqlrouter::ClusterType::RS_V2 ? "InnoDB ReplicaSet"
                                                        : "InnoDB Cluster";
    EXPECT_THAT(lines, ::testing::Contains(
                           "# MySQL Router configured for the " +
                           cluster_type_name + " '" + cluster_name + "'"));

    config_file = bootstrap_dir.name() + "/mysqlrouter.conf";

    // check that the config files (static and dynamic) have the proper
    // access rights
    ASSERT_NO_FATAL_FAILURE(
        check_config_file_access_rights(config_file, /*read_only=*/true));
    const std::string state_file = bootstrap_dir.name() + "/data/state.json";
    ASSERT_NO_FATAL_FAILURE(
        check_config_file_access_rights(state_file, /*read_only=*/false));
  }
}

void RouterComponentBootstrapWithDefaultCertsTest::SetUp() {
  RouterComponentTest::SetUp();

  prepare_config_dir_with_default_certs(bootstrap_dir.name());
}

std::ostream &operator<<(
    std::ostream &os,
    const std::vector<std::tuple<ProcessWrapper &, unsigned int>> &T) {
  for (auto &t : T) {
    auto &proc = std::get<0>(t);

    os << "member@" << std::to_string(std::get<1>(t)) << ": "
       << proc.get_current_output() << std::endl;
  }
  return os;
}
