#ifndef MYSQL_VAULT_IO_H
#define MYSQL_VAULT_IO_H

#include <boost/core/noncopyable.hpp>
#include "i_vault_io.h"
#include "plugin/keyring/common/secure_string.h"
#include "vault_key_serializer.h"

namespace keyring {
class ILogger;
class IVault_curl;
class IVault_parser_composer;

class Vault_io final : public IVault_io, private boost::noncopyable {
 public:
  Vault_io(ILogger *logger, IVault_curl *vault_curl,
           IVault_parser_composer *vault_parser)
      : logger(logger), vault_curl(vault_curl), vault_parser(vault_parser) {}

  ~Vault_io();

  virtual bool retrieve_key_type_and_data(IKey *key) override;

  bool init(const std::string *keyring_storage_url) override;
  bool flush_to_backup(
      ISerialized_object *serialized_object MY_ATTRIBUTE((unused))) override {
    return false;  // we do not have backup storage in vault
  }
  bool flush_to_storage(ISerialized_object *serialized_object) override;

  ISerializer *get_serializer() override;
  bool get_serialized_object(ISerialized_object **serialized_object) override;
  bool has_next_serialized_object() override { return false; }
  void set_curl_timeout(uint timeout) noexcept override;

 private:
  bool write_key(const Vault_key &key);
  bool delete_key(const Vault_key &key);
  Secure_string get_errors_from_response(const Secure_string &json_response);

  ILogger *logger;
  IVault_curl *vault_curl;
  IVault_parser_composer *vault_parser;
  Vault_key_serializer vault_key_serializer;
};

}  // namespace keyring

#endif  // MYSQL_VAULT_IO_H
