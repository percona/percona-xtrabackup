#ifndef MYSQL_GUNIT_VAULT_ENVIRONMENT_H
#define MYSQL_GUNIT_VAULT_ENVIRONMENT_H

#include <string>

#include <gtest/gtest.h>

class Vault_environment : public ::testing::Environment {
 public:
  Vault_environment();

  virtual ~Vault_environment() {}

  const std::string &get_uuid() const { return uuid_; }
  const std::string &get_key1_id() const { return key1_id_; }
  const std::string &get_key2_id() const { return key2_id_; }

  const char *get_uuid_raw() const { return uuid_.c_str(); }
  const char *get_key1_id_raw() const { return key1_id_.c_str(); }
  const char *get_key2_id_raw() const { return key2_id_.c_str(); }

  const std::string &get_default_conf_file_name() const {
    return default_conf_file_name_;
  }
  const std::string &get_invalid_conf_file_name() const {
    return invalid_conf_file_name_;
  }
  const std::string &get_non_existing_conf_file_name() const {
    return non_existing_conf_file_name_;
  }

  const std::string &get_mount_point_path() const { return mount_point_path_; }

  const std::string &get_admin_token() const { return admin_token_; }

  static std::string get_key_signature_ex(const std::string &uuid,
                                          const std::string &key_id,
                                          const std::string &user);

  std::string get_key_signature(const std::string &key_id,
                                const std::string &user) const {
    return get_key_signature_ex(uuid_, key_id, user);
  }

 protected:
  // Override this to define how to set up the environment.
  virtual void SetUp();

  // Override this to define how to tear down the environment.
  virtual void TearDown();

 private:
  std::string uuid_;
  std::string key1_id_;
  std::string key2_id_;

  std::string default_conf_file_name_;
  std::string invalid_conf_file_name_;
  std::string non_existing_conf_file_name_;

  std::string mount_point_path_;

  std::string get_conf_file_name(const std::string &base) const {
    return "./" + base + "_" + uuid_ + ".conf";
  }
};

#endif  // MYSQL_GUNIT_VAULT_ENVIRONMENT_H
