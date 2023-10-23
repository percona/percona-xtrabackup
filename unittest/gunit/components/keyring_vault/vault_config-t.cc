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
#include <fstream>
#include <memory>

#include "components/keyrings/keyring_vault/config/config.h"

namespace keyring_vault {
std::unique_ptr<config::Config_pod> g_config_pod;
}  // namespace keyring_vault

namespace keyring_vault_config_unittest {
namespace {
std::string config_path{"."};
}  // namespace

using Config_pod = keyring_vault::config::Config_pod;

using ::testing::StrEq;

class Vault_config_test : public ::testing::Test {
 protected:
  void SetUp() override {
    keyring_vault::config::g_component_path = strdup(config_path.c_str());
    keyring_vault::config::g_instance_path = strdup(config_path.c_str());
  }

  void TearDown() override {
    std::string path;
    keyring_vault::config::get_global_config_path(path);
    std::remove(path.c_str());
    free(keyring_vault::config::g_component_path);
    free(keyring_vault::config::g_instance_path);
  }

  static void create_empty_credentials_file(std::ofstream &my_file) {
    std::string path;
    keyring_vault::config::get_global_config_path(path);
    std::remove(path.c_str());
    my_file.open(path);
  }
};

TEST_F(Vault_config_test, ParseNotExistingFile) {
  std::string path;
  keyring_vault::config::get_global_config_path(path);
  std::remove(path.c_str());

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_TRUE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 0);
  EXPECT_TRUE(config_pod->vault_url.empty());
  EXPECT_TRUE(config_pod->token.empty());
  EXPECT_TRUE(config_pod->secret_mount_point.empty());
  EXPECT_TRUE(config_pod->vault_ca.empty());
}

TEST_F(Vault_config_test, ParseEmptyFile) {
  std::ofstream myfile;
  create_empty_credentials_file(myfile);
  myfile.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_TRUE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 0);
  EXPECT_TRUE(config_pod->vault_url.empty());
  EXPECT_TRUE(config_pod->token.empty());
  EXPECT_TRUE(config_pod->secret_mount_point.empty());
  EXPECT_TRUE(config_pod->vault_ca.empty());
}

TEST_F(Vault_config_test, ParseFileWithoutSecretMountPoint) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \"https://127.0.0.1:8200\"," << std::endl;
  my_file << "  \"token\": \"123-123-123\"," << std::endl;
  my_file << "  \"vault_ca\": \"/some/path\"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_TRUE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 0);
  EXPECT_TRUE(config_pod->vault_url.empty());
  EXPECT_TRUE(config_pod->token.empty());
  EXPECT_TRUE(config_pod->secret_mount_point.empty());
  EXPECT_TRUE(config_pod->vault_ca.empty());
}

TEST_F(Vault_config_test, ParseFileWithoutVaultURL) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"secret_mount_point\": \"secret\"," << std::endl;
  my_file << "  \"token\": \"123-123-123\"," << std::endl;
  my_file << "  \"vault_ca\": \"/some/path\"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_TRUE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 0);
  EXPECT_TRUE(config_pod->vault_url.empty());
  EXPECT_TRUE(config_pod->token.empty());
  EXPECT_TRUE(config_pod->secret_mount_point.empty());
  EXPECT_TRUE(config_pod->vault_ca.empty());
}

TEST_F(Vault_config_test, ParseFileWithoutToken) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \"https://127.0.0.1:8200\"," << std::endl;
  my_file << "  \"secret_mount_point\": \"secret\"," << std::endl;
  my_file << "  \"vault_ca\": \"/some/path\"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_TRUE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 0);
  EXPECT_TRUE(config_pod->vault_url.empty());
  EXPECT_TRUE(config_pod->token.empty());
  EXPECT_TRUE(config_pod->secret_mount_point.empty());
  EXPECT_TRUE(config_pod->vault_ca.empty());
}

TEST_F(Vault_config_test, ParseFileWithoutVaultCA) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \"http://127.0.0.1:8200\"," << std::endl;
  my_file << "  \"secret_mount_point\": \"secret\"," << std::endl;
  my_file << "  \"token\": \"123-123-123\"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_FALSE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 10);
  EXPECT_STREQ(config_pod->vault_url.c_str(), "http://127.0.0.1:8200");
  EXPECT_STREQ(config_pod->secret_mount_point.c_str(), "secret");
  EXPECT_STREQ(config_pod->token.c_str(), "123-123-123");
  EXPECT_TRUE(config_pod->vault_ca.empty());
}

TEST_F(Vault_config_test, ParseFileWithoutTimeout) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"vault_url\": \"https://127.0.0.1:8200\"," << std::endl;
  my_file << "  \"secret_mount_point\": \"secret\"," << std::endl;
  my_file << "  \"token\": \"123-123-123\"," << std::endl;
  my_file << "  \"vault_ca\": \"/some/path\"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_FALSE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 15);  // default value
  EXPECT_STREQ(config_pod->vault_url.c_str(), "https://127.0.0.1:8200");
  EXPECT_STREQ(config_pod->secret_mount_point.c_str(), "secret");
  EXPECT_STREQ(config_pod->token.c_str(), "123-123-123");
  EXPECT_STREQ(config_pod->vault_ca.c_str(), "/some/path");
}

TEST_F(Vault_config_test, ParseFileWithCorrectCredentials) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \"https://127.0.0.1:8200\"," << std::endl;
  my_file << "  \"secret_mount_point\": \"secret\"," << std::endl;
  my_file << "  \"token\": \"123-123-123\"," << std::endl;
  my_file << "  \"vault_ca\": \"/some/path\"," << std::endl;
  my_file << "  \"secret_mount_point_version\": \"1\"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_FALSE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 10);
  EXPECT_STREQ(config_pod->vault_url.c_str(), "https://127.0.0.1:8200");
  EXPECT_STREQ(config_pod->secret_mount_point.c_str(), "secret");
  EXPECT_STREQ(config_pod->token.c_str(), "123-123-123");
  EXPECT_STREQ(config_pod->vault_ca.c_str(), "/some/path");
  EXPECT_EQ(config_pod->secret_mount_point_version,
            keyring_vault::config::Vault_version_v1);
}

TEST_F(Vault_config_test,
       ParseFileWithCorrectCredentialsWithSecretMountPointVersion2) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \"https://127.0.0.1:8200\"," << std::endl;
  my_file << "  \"secret_mount_point\": \"secret\"," << std::endl;
  my_file << "  \"token\": \"123-123-123\"," << std::endl;
  my_file << "  \"vault_ca\": \"/some/path\"," << std::endl;
  my_file << "  \"secret_mount_point_version\": \"2\"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_FALSE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 10);
  EXPECT_STREQ(config_pod->vault_url.c_str(), "https://127.0.0.1:8200");
  EXPECT_STREQ(config_pod->secret_mount_point.c_str(), "secret");
  EXPECT_STREQ(config_pod->token.c_str(), "123-123-123");
  EXPECT_STREQ(config_pod->vault_ca.c_str(), "/some/path");
  EXPECT_EQ(config_pod->secret_mount_point_version,
            keyring_vault::config::Vault_version_v2);
}

TEST_F(Vault_config_test,
       ParseFileWithCorrectCredentialsWithSecretMountPointVersionAUTO) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \"https://127.0.0.1:8200\"," << std::endl;
  my_file << "  \"secret_mount_point\": \"secret\"," << std::endl;
  my_file << "  \"token\": \"123-123-123\"," << std::endl;
  my_file << "  \"vault_ca\": \"/some/path\"," << std::endl;
  my_file << "  \"secret_mount_point_version\": \"AUTO\"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_FALSE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 10);
  EXPECT_STREQ(config_pod->vault_url.c_str(), "https://127.0.0.1:8200");
  EXPECT_STREQ(config_pod->secret_mount_point.c_str(), "secret");
  EXPECT_STREQ(config_pod->token.c_str(), "123-123-123");
  EXPECT_STREQ(config_pod->vault_ca.c_str(), "/some/path");
  EXPECT_EQ(config_pod->secret_mount_point_version,
            keyring_vault::config::Vault_version_auto);
}

TEST_F(Vault_config_test,
       ParseFileWithCorrectCredentialsWithSecretMountPointVersionAUTO2) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \"https://127.0.0.1:8200\"," << std::endl;
  my_file << "  \"secret_mount_point\": \"secret\"," << std::endl;
  my_file << "  \"token\": \"123-123-123\"," << std::endl;
  my_file << "  \"vault_ca\": \"/some/path\"," << std::endl;
  my_file << "  \"secret_mount_point_version\": \"       AUTO   \""
          << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_FALSE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 10);
  EXPECT_STREQ(config_pod->vault_url.c_str(), "https://127.0.0.1:8200");
  EXPECT_STREQ(config_pod->secret_mount_point.c_str(), "secret");
  EXPECT_STREQ(config_pod->token.c_str(), "123-123-123");
  EXPECT_STREQ(config_pod->vault_ca.c_str(), "/some/path");
  EXPECT_EQ(config_pod->secret_mount_point_version,
            keyring_vault::config::Vault_version_auto);
}

TEST_F(Vault_config_test, ParseFileWithWithSecretMountVersionTooBig) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \"http://127.0.0.1:8200\"," << std::endl;
  my_file << "  \"secret_mount_point\": \"secret\"," << std::endl;
  my_file << "  \"token\": \"123-123-123\"," << std::endl;
  my_file << "  \"vault_ca\": \"/some/path\"," << std::endl;
  my_file << "  \"secret_mount_point_version\": \"3\"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_TRUE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 0);
  EXPECT_TRUE(config_pod->vault_url.empty());
  EXPECT_TRUE(config_pod->token.empty());
  EXPECT_TRUE(config_pod->secret_mount_point.empty());
  EXPECT_TRUE(config_pod->vault_ca.empty());
  EXPECT_EQ(config_pod->secret_mount_point_version,
            keyring_vault::config::Vault_version_unknown);
}

TEST_F(Vault_config_test, ParseFileWithSecretMountVersionTooBig2) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \"http://127.0.0.1:8200\"," << std::endl;
  my_file << "  \"secret_mount_point\": \"secret\"," << std::endl;
  my_file << "  \"token\": \"123-123-123\"," << std::endl;
  my_file << "  \"vault_ca\": \"/some/path\"," << std::endl;
  my_file << "  \"secret_mount_point_version\": \"1000000000000000000\""
          << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_TRUE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 0);
  EXPECT_TRUE(config_pod->vault_url.empty());
  EXPECT_TRUE(config_pod->token.empty());
  EXPECT_TRUE(config_pod->secret_mount_point.empty());
  EXPECT_TRUE(config_pod->vault_ca.empty());
  EXPECT_EQ(config_pod->secret_mount_point_version,
            keyring_vault::config::Vault_version_unknown);
}

TEST_F(Vault_config_test, ParseFileWithSecretMountVersionTooSmall2) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \"http://127.0.0.1:8200\"," << std::endl;
  my_file << "  \"secret_mount_point\": \"secret\"," << std::endl;
  my_file << "  \"token\": \"123-123-123\"," << std::endl;
  my_file << "  \"vault_ca\": \"/some/path\"," << std::endl;
  my_file << "  \"secret_mount_point_version\": \"-1\"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_TRUE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 0);
  EXPECT_TRUE(config_pod->vault_url.empty());
  EXPECT_TRUE(config_pod->token.empty());
  EXPECT_TRUE(config_pod->secret_mount_point.empty());
  EXPECT_TRUE(config_pod->vault_ca.empty());
  EXPECT_EQ(config_pod->secret_mount_point_version,
            keyring_vault::config::Vault_version_unknown);
}

TEST_F(Vault_config_test, ParseFileWithSecretMountVersionBeingNumeric) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \"http://127.0.0.1:8200\"," << std::endl;
  my_file << "  \"secret_mount_point\": \"secret\"," << std::endl;
  my_file << "  \"token\": \"123-123-123\"," << std::endl;
  my_file << "  \"vault_ca\": \"/some/path\"," << std::endl;
  my_file << "  \"secret_mount_point_version\": 1" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_TRUE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 0);
  EXPECT_TRUE(config_pod->vault_url.empty());
  EXPECT_TRUE(config_pod->token.empty());
  EXPECT_TRUE(config_pod->secret_mount_point.empty());
  EXPECT_TRUE(config_pod->vault_ca.empty());
  EXPECT_EQ(config_pod->secret_mount_point_version,
            keyring_vault::config::Vault_version_unknown);
}

TEST_F(Vault_config_test, ParseFileWithVaultCaBeingNumeric) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \"http://127.0.0.1:8200\"," << std::endl;
  my_file << "  \"secret_mount_point\": \"secret\"," << std::endl;
  my_file << "  \"token\": \"123-123-123\"," << std::endl;
  my_file << "  \"vault_ca\": 1," << std::endl;
  my_file << "  \"secret_mount_point_version\": \"1\"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_TRUE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 0);
  EXPECT_TRUE(config_pod->vault_url.empty());
  EXPECT_TRUE(config_pod->token.empty());
  EXPECT_TRUE(config_pod->secret_mount_point.empty());
  EXPECT_TRUE(config_pod->vault_ca.empty());
  EXPECT_EQ(config_pod->secret_mount_point_version,
            keyring_vault::config::Vault_version_unknown);
}

TEST_F(Vault_config_test, ParseFileWithTokenBeingNumeric) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \"http://127.0.0.1:8200\"," << std::endl;
  my_file << "  \"secret_mount_point\": \"secret\"," << std::endl;
  my_file << "  \"token\": 1," << std::endl;
  my_file << "  \"vault_ca\": \"/some/path\"," << std::endl;
  my_file << "  \"secret_mount_point_version\": \"1\"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_TRUE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 0);
  EXPECT_TRUE(config_pod->vault_url.empty());
  EXPECT_TRUE(config_pod->token.empty());
  EXPECT_TRUE(config_pod->secret_mount_point.empty());
  EXPECT_TRUE(config_pod->vault_ca.empty());
  EXPECT_EQ(config_pod->secret_mount_point_version,
            keyring_vault::config::Vault_version_unknown);
}

TEST_F(Vault_config_test, ParseFileWithMountPointBeingNumeric) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \"http://127.0.0.1:8200\"," << std::endl;
  my_file << "  \"secret_mount_point\": 1," << std::endl;
  my_file << "  \"token\": \"123-123-123\"," << std::endl;
  my_file << "  \"vault_ca\": \"/some/path\"," << std::endl;
  my_file << "  \"secret_mount_point_version\": \"1\"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_TRUE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 0);
  EXPECT_TRUE(config_pod->vault_url.empty());
  EXPECT_TRUE(config_pod->token.empty());
  EXPECT_TRUE(config_pod->secret_mount_point.empty());
  EXPECT_TRUE(config_pod->vault_ca.empty());
  EXPECT_EQ(config_pod->secret_mount_point_version,
            keyring_vault::config::Vault_version_unknown);
}

TEST_F(Vault_config_test, ParseFileWithVaultUrlBeingNumeric) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": 1," << std::endl;
  my_file << "  \"secret_mount_point\": \"secret\"," << std::endl;
  my_file << "  \"token\": \"123-123-123\"," << std::endl;
  my_file << "  \"vault_ca\": \"/some/path\"," << std::endl;
  my_file << "  \"secret_mount_point_version\": \"1\"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_TRUE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 0);
  EXPECT_TRUE(config_pod->vault_url.empty());
  EXPECT_TRUE(config_pod->token.empty());
  EXPECT_TRUE(config_pod->secret_mount_point.empty());
  EXPECT_TRUE(config_pod->vault_ca.empty());
  EXPECT_EQ(config_pod->secret_mount_point_version,
            keyring_vault::config::Vault_version_unknown);
}

TEST_F(Vault_config_test, ParseFileWithCorrectCredentialsSpaces) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \" https://127.0.0.1:8200 \"," << std::endl;
  my_file << "  \"secret_mount_point\":\" secret \"," << std::endl;
  my_file << "  \"token\": \" 123-123-123 \"," << std::endl;
  my_file << "  \"vault_ca\": \" /some/path \"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_FALSE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 10);
  EXPECT_STREQ(config_pod->vault_url.c_str(), "https://127.0.0.1:8200");
  EXPECT_STREQ(config_pod->secret_mount_point.c_str(), "secret");
  EXPECT_STREQ(config_pod->token.c_str(), "123-123-123");
  EXPECT_STREQ(config_pod->vault_ca.c_str(), "/some/path");
}

TEST_F(Vault_config_test, ParseFileWithValuesWithSpacesInIt) {
  std::ofstream my_file;
  create_empty_credentials_file(my_file);
  my_file << "{" << std::endl;
  my_file << "  \"timeout\": 10," << std::endl;
  my_file << "  \"vault_url\": \"https://127 .0.0.1: 8200 \"," << std::endl;
  my_file << "  \"secret_mount_point\": \"s-e c-r -e t \"," << std::endl;
  my_file << "  \"token\": \"12000 3-10  23- 123 \"," << std::endl;
  my_file << "  \"vault_ca\": \"/some/  path\"" << std::endl;
  my_file << "}" << std::endl;
  my_file.close();

  auto config_pod = std::make_unique<Config_pod>();
  EXPECT_FALSE(find_and_read_config_file(config_pod));

  EXPECT_TRUE(config_pod->timeout == 10);
  EXPECT_STREQ(config_pod->vault_url.c_str(), "https://127 .0.0.1: 8200");
  EXPECT_STREQ(config_pod->secret_mount_point.c_str(), "s-e c-r -e t");
  EXPECT_STREQ(config_pod->token.c_str(), "12000 3-10  23- 123");
  EXPECT_STREQ(config_pod->vault_ca.c_str(), "/some/  path");
}

}  // namespace keyring_vault_config_unittest
