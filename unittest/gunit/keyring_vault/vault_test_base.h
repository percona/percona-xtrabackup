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
