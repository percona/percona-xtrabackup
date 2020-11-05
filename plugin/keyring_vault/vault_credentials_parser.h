#ifndef MYSQL_VAULT_CREDENTIALS_PARSER_H
#define MYSQL_VAULT_CREDENTIALS_PARSER_H

#include <my_global.h>
#include <string>
#include <set>
#include "vault_credentials.h"
#include "logger.h"
#include <boost/optional.hpp>

namespace keyring {
class Vault_credentials_parser {
 public:
  Vault_credentials_parser(ILogger *logger) : logger(logger)
  {
    vault_credentials_in_progress.insert(std::make_pair("vault_url", ""));
    vault_credentials_in_progress.insert(
        std::make_pair("secret_mount_point", ""));
    vault_credentials_in_progress.insert(std::make_pair("vault_ca", ""));
    vault_credentials_in_progress.insert(std::make_pair("token", ""));
    vault_credentials_in_progress.insert(
        std::make_pair("secret_mount_point_version", ""));

    value_options.insert(
        std::make_pair("vault_url", new Value_options(false)));
    value_options.insert(
        std::make_pair("secret_mount_point", new Value_options(false)));
    value_options.insert(std::make_pair("vault_ca", new Value_options(true)));
    value_options.insert(std::make_pair("token", new Value_options(false)));
    value_options.insert(std::make_pair(
        "secret_mount_point_version",
        new Value_int_options(true, boost::make_optional<Secure_string>("1"),
                              boost::make_optional(1),
                              boost::make_optional(2))));
  }

  ~Vault_credentials_parser()
  {
    for (Value_options_map::iterator iter= value_options.begin();
         iter != value_options.end(); ++iter)
      delete iter->second;
  }

  bool parse(const std::string &file_url,
             Vault_credentials *vault_credentials);

 private:
  class Value_options {
   public:
    Value_options(bool                           is_optional,
                  boost::optional<Secure_string> default_value= boost::none)
        : is_optional(is_optional), default_value(default_value)
    {
    }
    virtual ~Value_options() {}

    bool                           is_optional;
    boost::optional<Secure_string> default_value;

    virtual bool process_value_options(const Secure_string &option_name,
                                       Secure_string &value, ILogger *logger);
  };

  class Value_int_options : public Value_options {
   public:
    Value_int_options(bool                           is_optional,
                      boost::optional<Secure_string> default_value,
                      boost::optional<int>           min_value,
                      boost::optional<int>           max_value)
        : Value_options(is_optional, default_value),
          min_value(min_value),
          max_value(max_value)
    {
    }

    virtual ~Value_int_options() {}

    virtual bool process_value_options(const Secure_string &option_name,
                                       Secure_string &value, ILogger *logger);

    boost::optional<int> min_value;
    boost::optional<int> max_value;
  };

  void reset_vault_credentials(Vault_credentials::Map *vault_credentials);

  bool parse_line(uint line_number, const Secure_string &line,
                  Vault_credentials::Map *vault_credentials_map);

  bool is_valid_option(const Secure_string &option) const;

  bool check_value_constrains(const Secure_string &option,
                              Secure_string &      value);

  Vault_credentials::Map vault_credentials_in_progress;

  typedef std::map<Secure_string, Value_options *> Value_options_map;
  Value_options_map                                value_options;

  ILogger *           logger;
  static const size_t max_file_size;
};

}  // namespace keyring

#endif  // MYSQL_VAULT_CREDENTIALS_PARSER_H
