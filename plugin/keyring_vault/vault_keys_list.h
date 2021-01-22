#ifndef MYSQL_VAULT_KEYS_H
#define MYSQL_VAULT_KEYS_H

#include <my_global.h>

#include <list>

#include <boost/core/noncopyable.hpp>

#include "i_serialized_object.h"

namespace keyring {

class Vault_keys_list : public ISerialized_object,
                        private boost::noncopyable {
 public:
  virtual my_bool get_next_key(IKey **key);
  virtual my_bool has_next_key();
  void            push_back(IKey *key);
  size_t          size() const;

  ~Vault_keys_list();

 private:
  typedef std::list<IKey *> Keys_list;
  Keys_list                 keys;
};

}  // namespace keyring

#endif  // MYSQL_VAULT_KEYS_H
