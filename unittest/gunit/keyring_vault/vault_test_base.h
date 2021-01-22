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
