#ifndef MYSQL_VAULT_KEYS_CONTAINER_H
#define MYSQL_VAULT_KEYS_CONTAINER_H

#include <boost/core/noncopyable.hpp>
#include "i_vault_io.h"
#include "plugin/keyring/common/keys_container.h"

namespace keyring {

class Vault_keys_container final : public Keys_container,
                                   private boost::noncopyable {
 public:
  Vault_keys_container(ILogger *logger) noexcept : Keys_container(logger) {}

  bool init(IKeyring_io *keyring_io, std::string keyring_storage_url) override;
  virtual IKey *fetch_key(IKey *key) override;
  virtual void set_curl_timeout(uint timeout) noexcept {
    DBUG_ASSERT(vault_io != NULL);
    vault_io->set_curl_timeout(timeout);
  }

 private:
  virtual bool flush_to_backup() override;
  IVault_io *vault_io;
};

}  // namespace keyring

#endif  // MYSQL_VAULT_KEYS_CONTAINER_H
