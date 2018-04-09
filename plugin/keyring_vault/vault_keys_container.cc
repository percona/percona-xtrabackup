#include <my_global.h>
#include "vault_keys_container.h"

namespace keyring
{
  my_bool Vault_keys_container::init(IKeyring_io* keyring_io, std::string keyring_storage_url)
  {
    vault_io = dynamic_cast<IVault_io*>(keyring_io);
    DBUG_ASSERT(vault_io != NULL);
    return Keys_container::init(keyring_io, keyring_storage_url);
  }

  IKey* Vault_keys_container::fetch_key(IKey *key)
  {
    DBUG_ASSERT(key->get_key_data() == NULL);
    DBUG_ASSERT(key->get_key_type()->empty());

    IKey *fetched_key= get_key_from_hash(key);

    if (fetched_key == NULL)
      return NULL;

    if(fetched_key->get_key_type()->empty() &&
       vault_io->retrieve_key_type_and_data(fetched_key)) // key is fetched for the first time
      return NULL;

    return Keys_container::fetch_key(key);
  }

  my_bool Vault_keys_container::flush_to_backup()
  {
    return FALSE;
  }
} // namespace keyring
