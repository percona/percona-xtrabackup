#ifndef MYSQL_VAULT_KEY_H
#define MYSQL_VAULT_KEY_H

#include "plugin/keyring/common/i_serialized_object.h"
#include "plugin/keyring/common/keyring_key.h"

namespace keyring {

struct Vault_key final : public Key, public ISerialized_object {
  Vault_key(const char *a_key_id, const char *a_key_type, const char *a_user_id,
            const void *a_key, size_t a_key_len) noexcept
      : Key(a_key_id, a_key_type, a_user_id, a_key, a_key_len),
        was_key_retrieved(false) {}

  Vault_key(const Vault_key &vault_key) noexcept
      : Key(vault_key.key_id.c_str(), vault_key.key_type.c_str(),
            vault_key.user_id.c_str(), vault_key.key.get(), vault_key.key_len),
        was_key_retrieved(false) {
    this->key_operation = vault_key.key_operation;
  }
  Vault_key() {}

  using Key::get_key_data;
  uchar *get_key_data() const;
  using Key::get_key_data_size;
  size_t get_key_data_size() const;
  using Key::get_key_type_as_string;
  const std::string *get_key_type_as_string() const;

  virtual bool get_next_key(IKey **key) override;
  virtual bool has_next_key() override;
  virtual void create_key_signature() const override;
  virtual void xor_data() override;

 protected:
  bool was_key_retrieved;
};

}  // namespace keyring

#endif  // MYSQL_VAULT_KEY_H
