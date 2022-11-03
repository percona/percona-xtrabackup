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

#include "vault_test_base.h"

#include <cstdio>

#include <curl/curl.h>

#include "generate_credential_file.h"
#include "mock_logger.h"
#include "vault_environment.h"
#include "vault_mount.h"

/*static*/ bool Vault_test_base::is_env_configured = false;
/*static*/ void *Vault_test_base::tc_curl_ = nullptr;
/*static*/ Vault_test_base::ilogger_ptr Vault_test_base::tc_logger_;
/*static*/ Vault_test_base::vault_mount_ptr Vault_test_base::tc_vault_mount_;

/*virtual*/ Vault_test_base::~Vault_test_base() {}

/*static*/ void Vault_test_base::SetUpTestCase() {
  if (!(is_env_configured = is_vault_environment_configured())) {
    return;
  }

  system_charset_info = &my_charset_utf8mb3_general_ci;

  ASSERT_FALSE(generate_credential_file(
      get_vault_env()->get_default_conf_file_name(),
      get_vault_env()->get_mount_point_path(),
      mount_point_version_type::mount_point_version_v1,
      credentials_validity_type::credentials_validity_correct))
      << "Could not generate credential file";

  tc_curl_ = curl_easy_init();
  ASSERT_TRUE(tc_curl_ != nullptr) << "Could not initialize CURL session";

  tc_logger_.reset(new keyring::Mock_logger);
  ASSERT_TRUE(tc_logger_ != nullptr)
      << "Could not create Test Case Logger object";

  tc_vault_mount_.reset(new keyring::Vault_mount(static_cast<CURL *>(tc_curl_),
                                                 tc_logger_.get()));
  ASSERT_TRUE(tc_vault_mount_ != nullptr)
      << "Could not create Vault_mount object";

  ASSERT_FALSE(
      tc_vault_mount_->init(get_vault_env()->get_default_conf_file_name(),
                            get_vault_env()->get_mount_point_path()))
      << "Could not initialized Vault_mount";

  ASSERT_FALSE(tc_vault_mount_->mount_secret_backend())
      << "Could not mount secret backend";
}

/*static*/ void Vault_test_base::TearDownTestCase() {
  if (!is_env_configured) {
    return;
  }

  // remove unique secret mount point
  ASSERT_FALSE(tc_vault_mount_->unmount_secret_backend())
      << "Could not unmount secret backend";

  tc_vault_mount_.reset();

  tc_logger_.reset();
  curl_easy_cleanup(static_cast<CURL *>(tc_curl_));
  tc_curl_ = nullptr;

  std::remove(get_vault_env()->get_default_conf_file_name().c_str());
}

keyring::ILogger *Vault_test_base::get_logger() const { return logger_.get(); }

/*virtual*/ void Vault_test_base::SetUp() {
  logger_.reset(new keyring::Mock_logger);
  ASSERT_TRUE(logger_ != nullptr) << "Could not create Test Logger object";
}

/*virtual*/ void Vault_test_base::TearDown() { logger_.reset(); }

Vault_environment *Vault_test_base::get_vault_env() {
  static auto *env = static_cast<Vault_environment *>(
      ::testing::AddGlobalTestEnvironment(new Vault_environment()));
  return env;
}
