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

#include "vault_test_base.h"

#include <cstdio>

#include <curl/curl.h>

#include "generate_credential_file.h"
#include "mock_logger.h"
#include "vault_environment.h"
#include "vault_mount.h"

/*static*/ void *                           Vault_test_base::tc_curl_= NULL;
/*static*/ Vault_test_base::ilogger_ptr     Vault_test_base::tc_logger_;
/*static*/ Vault_test_base::vault_mount_ptr Vault_test_base::tc_vault_mount_;

/*virtual*/ Vault_test_base::~Vault_test_base() {}

/*static*/ void Vault_test_base::SetUpTestCase()
{
  ASSERT_FALSE(generate_credential_file(
      Vault_environment::get_instance()->get_default_conf_file_name(),
      Vault_environment::get_instance()->get_mount_point_path(),
      mount_point_version_v1, credentials_validity_correct))
      << "Could not generate credential file";

  tc_curl_= curl_easy_init();
  ASSERT_TRUE(tc_curl_ != NULL) << "Could not initialize CURL session";

  tc_logger_.reset(new keyring::Mock_logger);
  ASSERT_TRUE(tc_logger_ != NULL)
      << "Could not create Test Case Logger object";

  tc_vault_mount_.reset(new keyring::Vault_mount(
      static_cast<CURL *>(tc_curl_), tc_logger_.get()));
  ASSERT_TRUE(tc_vault_mount_ != NULL)
      << "Could not create Vault_mount object";

  ASSERT_FALSE(tc_vault_mount_->init(
      Vault_environment::get_instance()->get_default_conf_file_name(),
      Vault_environment::get_instance()->get_mount_point_path(),
      Vault_environment::get_instance()->get_admin_token()))
      << "Could not initialized Vault_mount";

  ASSERT_FALSE(tc_vault_mount_->mount_secret_backend())
      << "Could not mount secret backend";
}

/*static*/ void Vault_test_base::TearDownTestCase()
{
  //remove unique secret mount point
  ASSERT_FALSE(tc_vault_mount_->unmount_secret_backend())
      << "Could not unmount secret backend";

  tc_vault_mount_.reset();

  tc_logger_.reset();
  curl_easy_cleanup(static_cast<CURL *>(tc_curl_));
  tc_curl_= NULL;

  std::remove(Vault_environment::get_instance()
                  ->get_default_conf_file_name()
                  .c_str());
}

keyring::ILogger *Vault_test_base::get_logger() const
{
  return logger_.get();
}

/*virtual*/ void Vault_test_base::SetUp()
{
  logger_.reset(new keyring::Mock_logger);
  ASSERT_TRUE(logger_ != NULL) << "Could not create Test Logger object";
}

/*virtual*/ void Vault_test_base::TearDown() { logger_.reset(); }
