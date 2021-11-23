/* Copyright (c) 2018, 2021 Percona LLC and/or its affiliates. All rights reserved.

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

#include <boost/scoped_ptr.hpp>

#include <gtest/gtest.h>

namespace keyring {
class Vault_mount;
class ILogger;
}  // namespace keyring

class Vault_test_base : public ::testing::Test {
 public:
  virtual ~Vault_test_base();

 protected:
  static void SetUpTestCase();
  static void TearDownTestCase();

  keyring::ILogger *get_logger() const;

  virtual void SetUp();
  virtual void TearDown();

 private:
  static void *                                   tc_curl_;
  typedef boost::scoped_ptr<keyring::ILogger>     ilogger_ptr;
  static ilogger_ptr                              tc_logger_;
  typedef boost::scoped_ptr<keyring::Vault_mount> vault_mount_ptr;
  static vault_mount_ptr                          tc_vault_mount_;

  ilogger_ptr logger_;
};

#endif  // MYSQL_GUNIT_VAULT_TEST_BASE_H
