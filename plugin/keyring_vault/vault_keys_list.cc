#include "vault_keys_list.h"

namespace keyring
{

// The caller takes ownership of the key, thus it is
// his resposibility to free the key	
my_bool Vault_keys_list::get_next_key(IKey **key)
{
  *key= NULL;
  if (size() == 0)
    return TRUE;
  *key = keys.front();
  keys.pop_front();
  return FALSE;
}

my_bool Vault_keys_list::has_next_key()
{
  return size() != 0;
}

size_t Vault_keys_list::size() const
{
  return keys.size();
}

Vault_keys_list::~Vault_keys_list()
{
  // remove not fetched keys
  for (Keys_list::iterator iter = keys.begin(); iter != keys.end();
       ++iter)
    delete *iter;
}

void Vault_keys_list::push_back(IKey* key)
{
  keys.push_back(key);
}

} // namespace keyring
