#ifndef MYSQL_VAULT_KEYS_CONTAINER_H
#define MYSQL_VAULT_KEYS_CONTAINER_H

#include <my_global.h>
#include <boost/core/noncopyable.hpp>
#include "keys_container.h"
#include "i_vault_io.h"

namespace keyring
{

class Vault_keys_container : public Keys_container, private boost::noncopyable
{
public:
  Vault_keys_container(ILogger* logger)
    : Keys_container(logger)
  {}

  my_bool init(IKeyring_io* keyring_io, std::string keyring_storage_url);
  virtual IKey* fetch_key(IKey *key);
  virtual void set_curl_timeout(uint timeout)
  {
    DBUG_ASSERT(vault_io != NULL);
    vault_io->set_curl_timeout(timeout);
  }

private:
  virtual my_bool flush_to_backup();
  IVault_io *vault_io;
};

} // namespace keyring

#endif // MYSQL_VAULT_KEYS_CONTAINER_H
