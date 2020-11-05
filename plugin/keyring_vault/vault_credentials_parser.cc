#include "vault_credentials_parser.h"
#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional/optional_io.hpp>
#include <boost/scope_exit.hpp>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include "plugin/keyring/file_io.h"

namespace keyring {
const size_t Vault_credentials_parser::max_file_size = 10000;

void Vault_credentials_parser::reset_vault_credentials(
    Vault_credentials::Map *vault_credentials) noexcept {
  for (auto &creds_pair : *vault_credentials) creds_pair.second.clear();
}

bool Vault_credentials_parser::is_valid_option(
    const Secure_string &option) const noexcept {
  return vault_credentials_in_progress.count(option) != 0;
}

bool Vault_credentials_parser::parse_line(
    uint line_number, const Secure_string &line,
    Vault_credentials::Map *vault_credentials) {
  if (line.empty()) return false;

  size_t eq_sign_pos = line.find('=');
  std::ostringstream err_ss;

  if (eq_sign_pos == std::string::npos) {
    err_ss << "Could not parse credential file. Cannot find equal sign (=) in "
              "line: ";
    err_ss << line_number << '.';
    logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
    return true;
  }
  Secure_string option = line.substr(0, eq_sign_pos);
  boost::trim(option);

  if (!is_valid_option(option)) {
    err_ss << "Could not parse credential file. Unknown option \"" << option
           << "\" in line: ";
    err_ss << line_number << '.';
    return true;
  }
  Secure_string *value = &(*vault_credentials)[option];

  if (!value->empty())  // repeated option in file
  {
    err_ss << "Could not parse credential file. Seems that value for option "
           << option;
    err_ss << " has been specified more than once in line: " << line_number
           << '.';
    logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
    return true;
  }
  *value = line.substr(eq_sign_pos + 1, line.size() - (eq_sign_pos + 1));
  boost::trim(*value);

  if (value->empty()) {
    err_ss << "Could not parse credential file. Seems there is no value "
              "specified ";
    err_ss << "for option " << option << " in line: " << line_number << '.';

    logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
    return true;
  }
  return false;
}

bool Vault_credentials_parser::Value_options::process_value_options(
    const Secure_string &option_name, Secure_string &value, ILogger *logger) {
  if (value.empty()) {
    if (!is_optional) {
      std::ostringstream err_ss;
      err_ss << "Could not read " << option_name
             << " from the configuration file.";
      logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
      return true;
    }
    if (default_value) {
      value = *default_value;
    }
  }
  return false;
}

bool Vault_credentials_parser::Value_int_options::process_value_options(
    const Secure_string &option_name, Secure_string &value, ILogger *logger) {
  if (Value_options::process_value_options(option_name, value, logger))
    return true;

  if (value == "AUTO") return false;

  if (min_value || max_value) {
    int int_value = -1;
    try {
      int_value = boost::lexical_cast<int>(value);
    } catch (const boost::bad_lexical_cast &) {
      std::ostringstream err_ss;
      err_ss << "Could not parse value for " << option_name
             << " from the configuration file.";
      logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
      return true;
    }
    if (int_value < min_value) {
      std::ostringstream err_ss;
      err_ss << "Incorrect value for " << option_name
             << ". It is smaller than min value =" << min_value << '.';
      logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
      return true;
    }
    if (int_value > max_value) {
      std::ostringstream err_ss;
      err_ss << "Incorrect value for " << option_name
             << ". It is greater than max value =" << max_value << '.';
      logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
      return true;
    }
  }
  return false;
}

bool Vault_credentials_parser::parse(const std::string &file_url,
                                     Vault_credentials *vault_credentials) {
  reset_vault_credentials(&vault_credentials_in_progress);

  File_io file_io(logger);
  File file = file_io.open(PSI_NOT_INSTRUMENTED, file_url.c_str(), O_RDONLY,
                           MYF(MY_WME));

  if (file < 0) {
    logger->log(MY_ERROR_LEVEL, "Could not open file with credentials.");
    return true;
  }
  BOOST_SCOPE_EXIT(&file_io, &file) { file_io.close(file, MYF(MY_WME)); }
  BOOST_SCOPE_EXIT_END

  if (file_io.seek(file, 0, MY_SEEK_END, MYF(MY_WME)) == MY_FILEPOS_ERROR) {
    logger->log(MY_ERROR_LEVEL, "Could not read file with credentials.");
    return true;
  }
  my_off_t file_size = file_io.tell(file, MYF(MY_WME));
  if (file_size == 0) {
    logger->log(MY_ERROR_LEVEL, "Empty file with credentials.");
    return true;
  }
  if (file_size > max_file_size ||
      file_io.seek(file, 0, MY_SEEK_SET, MYF(MY_WME)) == MY_FILEPOS_ERROR) {
    logger->log(MY_ERROR_LEVEL, "Could not read file with credentials.");
    return true;
  }
  std::unique_ptr<uchar[]> buffer(new uchar[file_size]);
  if (file_io.read(file, buffer.get(), file_size, MYF(MY_WME)) != file_size) {
    logger->log(MY_ERROR_LEVEL, "Could not read file with credentials.");
    return true;
  }
  Secure_string file_content(reinterpret_cast<char *>(buffer.get()), file_size);
  Secure_istringstream credentials_ss(file_content);
  uint line_number = 1;
  Secure_string line;
  while (!getline(credentials_ss, line).fail())
    if (parse_line(line_number, line, &vault_credentials_in_progress)) {
      line_number++;
      return true;
    }

  for (auto &creds_pair : vault_credentials_in_progress) {
    DBUG_ASSERT(value_options.count(creds_pair.first) != 0);

    if (value_options[creds_pair.first]->process_value_options(
            creds_pair.first, creds_pair.second, logger))
      return true;
  }
  vault_credentials->init(vault_credentials_in_progress);
  return false;
}
}  // namespace keyring
