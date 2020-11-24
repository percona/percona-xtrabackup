#ifndef MYSQL_VAULT_KEY_SERIALIZER_H
#define MYSQL_VAULT_KEY_SERIALIZER_H

#include "my_dbug.h"
#include "plugin/keyring/common/i_serializer.h"
#include "vault_key.h"

namespace keyring {

class Vault_key_serializer final : public ISerializer {
 public:
  virtual ISerialized_object *serialize(
      const collation_unordered_map<std::string, std::unique_ptr<IKey>>
          &keys_hash MY_ATTRIBUTE((unused)),
      IKey *key, const Key_operation operation) override {
    Vault_key *vault_key = dynamic_cast<Vault_key *>(key);
    DBUG_ASSERT(vault_key != nullptr);
    vault_key->set_key_operation(operation);

    return new Vault_key(*vault_key);
  }
};

}  // namespace keyring

#endif  // MYSQL_VAULT_KEY_SERIALIZER_H
