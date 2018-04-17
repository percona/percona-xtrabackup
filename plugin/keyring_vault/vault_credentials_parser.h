#ifndef MYSQL_VAULT_CREDENTIALS_PARSER_H
#define MYSQL_VAULT_CREDENTIALS_PARSER_H

#include <my_global.h>
#include <string>
#include <set>
#include "vault_credentials.h"
#include "logger.h"

namespace keyring
{
  class Vault_credentials_parser
  {
  public:
    Vault_credentials_parser(ILogger *logger)
      : logger(logger)
    {
      vault_credentials_in_progress.insert(std::make_pair("vault_url", ""));
      vault_credentials_in_progress.insert(std::make_pair("secret_mount_point", ""));
      vault_credentials_in_progress.insert(std::make_pair("vault_ca", ""));
      vault_credentials_in_progress.insert(std::make_pair("token", ""));

      optional_value.insert("vault_ca");
    }

    bool parse(const std::string &file_url, Vault_credentials *vault_credentials);

  private:
    void reset_vault_credentials(Vault_credentials *vault_credentials);

    bool parse_line(uint line_number, const Secure_string& line, Vault_credentials *vault_credentials);

    bool is_valid_option(const Secure_string &option) const;
    Vault_credentials vault_credentials_in_progress;
    std::set<Secure_string> optional_value;

    ILogger *logger;
    static const size_t max_file_size;
  };

} // namespace keyring

#endif // MYSQL_VAULT_CREDENTIALS_PARSER_H
