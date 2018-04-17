#ifndef MYSQL_VAULT_KEYS_CONTAINER_EX_H
#define MYSQL_VAULT_KEYS_CONTAINER_EX_H

#include <my_global.h>
#include "vault_keys_container.h"
#include "hash.h"

namespace keyring {

class Vault_keys_container_ex : public Vault_keys_container
{
public:
  Vault_keys_container_ex(ILogger* logger)
    : Vault_keys_container(logger)
  {}
  
  void remove_all_keys()
  {
    for (ulong idx = 0; idx < keys_hash->records; idx++)
    {
      IKey *key = reinterpret_cast<IKey*>(my_hash_element(keys_hash, idx));
      remove_key(key);    
    }
  }
};

} // namespace keyring

#endif // MYSQL_VAULT_KEYS_CONTAINER_EX_H
