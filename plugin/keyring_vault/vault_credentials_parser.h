#ifndef MYSQL_VAULT_CREDENTIALS_PARSER_H
#define MYSQL_VAULT_CREDENTIALS_PARSER_H

#include <cstddef>

#include <string>

namespace keyring {
class ILogger;
class Vault_credentials;

class Vault_credentials_parser final {
 public:
  explicit Vault_credentials_parser(ILogger *logger) : logger_(logger) {}

  bool parse(const std::string &conf_file_path,
             Vault_credentials &vault_credentials) const;

 private:
  ILogger *logger_;

  static const std::size_t max_file_size = 16384;
};

}  // namespace keyring

#endif  // MYSQL_VAULT_CREDENTIALS_PARSER_H
