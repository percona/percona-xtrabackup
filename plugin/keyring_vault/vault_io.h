#ifndef MYSQL_VAULT_IO_H
#define MYSQL_VAULT_IO_H

#include <my_global.h>
#include <boost/core/noncopyable.hpp>
#include <logger.h>
#include "i_vault_io.h"
#include "vault_parser.h"
#include "vault_curl.h"
#include "vault_key_serializer.h"

namespace keyring {

class Vault_io : public IVault_io, private boost::noncopyable
{
public:
  Vault_io(ILogger *logger, IVault_curl *vault_curl,
           IVault_parser *vault_parser)
    : logger(logger)
    , vault_curl(vault_curl)
    , vault_parser(vault_parser)
  {}

  ~Vault_io();

  virtual my_bool retrieve_key_type_and_data(IKey *key);

  virtual my_bool init(std::string *keyring_storage_url);
  virtual my_bool flush_to_backup(ISerialized_object *serialized_object)
  {
    return FALSE; // we do not have backup storage in vault
  }
  virtual my_bool flush_to_storage(ISerialized_object *serialized_object);

  virtual ISerializer *get_serializer();
  virtual my_bool get_serialized_object(ISerialized_object **serialized_object);
  virtual my_bool has_next_serialized_object()
  {
    return FALSE;
  }
  virtual void set_curl_timeout(uint timeout)
  {
    DBUG_ASSERT(vault_curl != NULL);
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

} // namespace keyring

#endif // MYSQL_VAULT_IO_H
