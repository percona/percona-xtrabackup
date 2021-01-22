#include "vault_environment.h"

#include <curl/curl.h>

#include "generate_credential_file.h"

Vault_environment::Vault_environment()
    : uuid_(generate_uuid()),
      key1_id_(uuid_ + "key1"),
      key2_id_(uuid_ + "key2"),
      default_conf_file_name_(get_conf_file_name("keyring_vault")),
      invalid_conf_file_name_(get_conf_file_name("invalid_token")),
      non_existing_conf_file_name_(get_conf_file_name("non_existing")),
      mount_point_path_("mtr/" + uuid_) {}

/*static*/ std::string Vault_environment::get_key_signature_ex(
    const std::string &uuid, const std::string &key_id,
    const std::string &user) {
  std::string id = uuid + key_id;
  std::ostringstream signature;
  signature << id.length() << '_' << id << user.length() << '_' << user;
  return signature.str();
}

/*virtual*/ void Vault_environment::SetUp() {
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

// Override this to define how to tear down the environment.
/*virtual*/ void Vault_environment::TearDown() { curl_global_cleanup(); }
