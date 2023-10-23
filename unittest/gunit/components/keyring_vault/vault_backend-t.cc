/* Copyright (c) 2023 Percona LLC and/or its affiliates. All rights
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "components/keyrings/common/data/data.h"
#include "components/keyrings/common/data/meta.h"
#include "components/keyrings/common/data/pfs_string.h"
#include "components/keyrings/keyring_vault/backend/backend.h"
#include "components/keyrings/keyring_vault/backend/i_vault_curl.h"
#include "components/keyrings/keyring_vault/config/config.h"

namespace keyring_vault_backend_unittest {

using Keyring_vault_backend = keyring_vault::backend::Keyring_vault_backend;
using Metadata = keyring_common::meta::Metadata;
using Data = keyring_common::data::Data;
using Sensitive_data = keyring_common::data::Sensitive_data;

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

class Mock_keyring_vault_curl
    : public keyring_vault::backend::IKeyring_vault_curl {
 public:
  MOCK_METHOD0(init, bool());
  MOCK_METHOD1(list_keys, bool(pfs_string *response));
  MOCK_METHOD3(write_key, bool(const Metadata &key, const Data &data,
                               pfs_string *response));
  MOCK_METHOD2(read_key, bool(const Metadata &key, pfs_string *response));
  MOCK_METHOD2(delete_key, bool(const Metadata &key, pfs_string *response));
  MOCK_METHOD1(set_timeout, void(uint timeout));
  MOCK_CONST_METHOD0(get_resolved_secret_mount_point_version,
                     keyring_vault::config::Vault_version_type());
};

class Vault_backend_test : public ::testing::Test {
 protected:
  static Mock_keyring_vault_curl *create_mock_vault_curl() {
    return new Mock_keyring_vault_curl;
  }
};

TEST_F(Vault_backend_test, InitWithNullConfigCurl) {
  Keyring_vault_backend vault_backend(nullptr);
  EXPECT_TRUE(vault_backend.init());
  EXPECT_FALSE(vault_backend.valid());
}

TEST_F(Vault_backend_test, InitWithInvalidVaultCurl) {
  auto *mock_vault_curl = create_mock_vault_curl();
  auto vault_backend = Keyring_vault_backend(
      std::unique_ptr<keyring_vault::backend::IKeyring_vault_curl>(
          mock_vault_curl));

  EXPECT_CALL(*mock_vault_curl, init()).WillOnce(Return(true));
  EXPECT_TRUE(vault_backend.init());
  EXPECT_FALSE(vault_backend.valid());
  EXPECT_TRUE(vault_backend.size() == 0);
}

TEST_F(Vault_backend_test, StoreEraseOk) {
  auto *mock_vault_curl = create_mock_vault_curl();
  auto vault_backend = Keyring_vault_backend(
      std::unique_ptr<keyring_vault::backend::IKeyring_vault_curl>(
          mock_vault_curl));

  auto metadata = Metadata{"key_id_1", "owner_id_1"};
  auto data = Data{Sensitive_data{"data_1"}, "type_1"};
  pfs_string json_response;

  EXPECT_CALL(*mock_vault_curl, init()).WillOnce(Return(false));
  EXPECT_CALL(*mock_vault_curl, list_keys(_))
      .WillOnce(DoAll(SetArgPointee<0>(json_response), Return(false)));
  EXPECT_CALL(*mock_vault_curl, write_key(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(json_response), Return(false)));
  EXPECT_CALL(*mock_vault_curl, delete_key(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(json_response), Return(false)));

  EXPECT_FALSE(vault_backend.init());
  EXPECT_FALSE(vault_backend.store(metadata, data));
  EXPECT_TRUE(vault_backend.size() == 1);

  EXPECT_FALSE(vault_backend.erase(metadata, data));
  EXPECT_TRUE(vault_backend.size() == 0);
}

TEST_F(Vault_backend_test, GenerateOk) {
  auto *mock_vault_curl = create_mock_vault_curl();
  auto vault_backend = Keyring_vault_backend(
      std::unique_ptr<keyring_vault::backend::IKeyring_vault_curl>(
          mock_vault_curl));

  auto metadata = Metadata{"key_id_1", "owner_id_1"};
  auto data = Data{};
  pfs_string json_response;

  EXPECT_CALL(*mock_vault_curl, init()).WillOnce(Return(false));
  EXPECT_CALL(*mock_vault_curl, list_keys(_))
      .WillOnce(DoAll(SetArgPointee<0>(json_response), Return(false)));
  EXPECT_CALL(*mock_vault_curl, write_key(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(json_response), Return(false)));

  EXPECT_FALSE(vault_backend.init());
  EXPECT_FALSE(vault_backend.generate(metadata, data, 100));
  EXPECT_TRUE(vault_backend.size() == 1);
  EXPECT_TRUE(data.valid());
}

}  // namespace keyring_vault_backend_unittest
