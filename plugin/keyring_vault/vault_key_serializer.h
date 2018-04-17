#ifndef MYSQL_VAULT_KEY_SERIALIZER_H
#define MYSQL_VAULT_KEY_SERIALIZER_H

#include "i_serializer.h"
#include "vault_key.h"

namespace keyring
{

class Vault_key_serializer :  public ISerializer
{
public:
  ISerialized_object* serialize(HASH *keys_hash, IKey *key,
                                const Key_operation operation)
  {
    Vault_key* vault_key = dynamic_cast<Vault_key*>(key);
    DBUG_ASSERT(vault_key != NULL);
    vault_key->set_key_operation(operation);

    return new Vault_key(*vault_key);
  }
};

} // namespace keyring

#endif // MYSQL_VAULT_KEY_SERIALIZER_H
