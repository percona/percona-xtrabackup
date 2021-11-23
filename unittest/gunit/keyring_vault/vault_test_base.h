/* Copyright (c) 2018, 2021 Percona LLC and/or its affiliates. All rights
   reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef MYSQL_GUNIT_VAULT_TEST_BASE_H
#define MYSQL_GUNIT_VAULT_TEST_BASE_H

#include <gtest/gtest.h>

class Vault_environment;

namespace keyring {
class Vault_mount;
class ILogger;
}  // namespace keyring

class Vault_test_base : public ::testing::Test {
 public:
  ~Vault_test_base() override;

  static bool check_env_configured() { return is_env_configured; }

 protected:
  static void SetUpTestCase();
  static void TearDownTestCase();

  keyring::ILogger *get_logger() const;

  void SetUp() override;
  void TearDown() override;

  static Vault_environment *get_vault_env();

 private:
  using ilogger_ptr = std::unique_ptr<keyring::ILogger>;
  using vault_mount_ptr = std::unique_ptr<keyring::Vault_mount>;

  static void *tc_curl_;
  static ilogger_ptr tc_logger_;
  static vault_mount_ptr tc_vault_mount_;

  ilogger_ptr logger_;
  static bool is_env_configured;
};

#endif  // MYSQL_GUNIT_VAULT_TEST_BASE_H
