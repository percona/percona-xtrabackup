#ifndef MYSQL_VAULT_CREDENTIALS_PARSER_H
#define MYSQL_VAULT_CREDENTIALS_PARSER_H

#include <boost/optional.hpp>
#include <set>
#include <string>
#include "plugin/keyring/common/logger.h"
#include "vault_credentials.h"

namespace keyring {
class Vault_credentials_parser final {
 public:
  Vault_credentials_parser(ILogger *logger) : logger(logger) {
    vault_credentials_in_progress.emplace(std::make_pair("vault_url", ""));
    vault_credentials_in_progress.emplace(
        std::make_pair("secret_mount_point", ""));
    vault_credentials_in_progress.emplace(std::make_pair("vault_ca", ""));
    vault_credentials_in_progress.emplace(std::make_pair("token", ""));
    vault_credentials_in_progress.emplace(
        std::make_pair("secret_mount_point_version", ""));

    value_options.emplace(
        std::make_pair("vault_url", new Value_options(false)));
    value_options.emplace(
        std::make_pair("secret_mount_point", new Value_options(false)));
    value_options.emplace(std::make_pair("vault_ca", new Value_options(true)));
    value_options.emplace(std::make_pair("token", new Value_options(false)));
    value_options.emplace(std::make_pair(
        "secret_mount_point_version",
        new Value_int_options(true, boost::make_optional<Secure_string>("1"),
                              boost::make_optional(1),
                              boost::make_optional(2))));
  }

  ~Vault_credentials_parser() {
    for (auto &options_pair : value_options) delete options_pair.second;
  }

  bool parse(const std::string &file_url, Vault_credentials *vault_credentials);

 private:
  class Value_options {
   public:
    explicit Value_options(
        bool is_optional,
        boost::optional<Secure_string> default_value = boost::none)
        : is_optional(is_optional), default_value(std::move(default_value)) {}
    virtual ~Value_options() = default;

    bool is_optional;
    boost::optional<Secure_string> default_value;

    virtual bool process_value_options(const Secure_string &option_name,
                                       Secure_string &value, ILogger *logger);
  };

  class Value_int_options : public Value_options {
   public:
    Value_int_options(bool is_optional,
                      boost::optional<Secure_string> default_value,
                      boost::optional<int> min_value,
                      boost::optional<int> max_value)
        : Value_options(is_optional, std::move(default_value)),
          min_value(min_value),
          max_value(max_value) {}

    ~Value_int_options() override = default;

    bool process_value_options(const Secure_string &option_name,
                               Secure_string &value, ILogger *logger) override;

    boost::optional<int> min_value;
    boost::optional<int> max_value;
  };

  static void reset_vault_credentials(
      Vault_credentials::Map *vault_credentials) noexcept;

  bool parse_line(uint line_number, const Secure_string &line,
                  Vault_credentials::Map *vault_credentials_map);

  bool is_valid_option(const Secure_string &option) const noexcept;

  bool check_value_constrains(const Secure_string &option,
                              Secure_string &value);

  Vault_credentials::Map vault_credentials_in_progress;

  typedef std::map<Secure_string, Value_options *> Value_options_map;
  Value_options_map value_options;

  ILogger *logger;
  static const size_t max_file_size;
};

}  // namespace keyring

#endif  // MYSQL_VAULT_CREDENTIALS_PARSER_H
