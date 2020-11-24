#ifndef MYSQL_VAULT_IO_H
#define MYSQL_VAULT_IO_H

#include <boost/core/noncopyable.hpp>
#include "i_vault_io.h"
#include "plugin/keyring/common/logger.h"
#include "vault_curl.h"
#include "vault_key_serializer.h"
#include "vault_parser.h"

namespace keyring {

class Vault_io final : public IVault_io, private boost::noncopyable {
 public:
  Vault_io(ILogger *logger, IVault_curl *vault_curl,
           IVault_parser *vault_parser) noexcept
      : logger(logger), vault_curl(vault_curl), vault_parser(vault_parser) {}

  ~Vault_io();

  virtual bool retrieve_key_type_and_data(IKey *key) override;

  virtual bool init(std::string *keyring_storage_url) override;
  virtual bool flush_to_backup(
      ISerialized_object *serialized_object MY_ATTRIBUTE((unused))) override {
    return false;  // we do not have backup storage in vault
  }
  virtual bool flush_to_storage(ISerialized_object *serialized_object) override;

  virtual ISerializer *get_serializer() override;
  virtual bool get_serialized_object(
      ISerialized_object **serialized_object) override;
  virtual bool has_next_serialized_object() override { return false; }
  virtual void set_curl_timeout(uint timeout) noexcept override {
    DBUG_ASSERT(vault_curl != nullptr);
    vault_curl->set_timeout(timeout);
  }

 private:
  bool write_key(const Vault_key &key);
  bool delete_key(const Vault_key &key);

  Secure_string get_errors_from_response(const Secure_string &json_response);

  ILogger *logger;
  IVault_curl *vault_curl;
  IVault_parser *vault_parser;
  Vault_key_serializer vault_key_serializer;
};

}  // namespace keyring

#endif  // MYSQL_VAULT_IO_H
