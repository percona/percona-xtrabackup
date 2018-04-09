#include <my_global.h>
#include "vault_credentials_parser.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <functional>
#include "boost/algorithm/string/trim.hpp"
#include "boost/move/unique_ptr.hpp"
#include "boost/scope_exit.hpp"
#include "file_io.h"

namespace keyring
{
  const size_t Vault_credentials_parser::max_file_size = 10000;

  void Vault_credentials_parser::reset_vault_credentials(Vault_credentials *vault_credentials)
  {
    for (Vault_credentials::iterator iter = vault_credentials->begin();
        iter != vault_credentials->end(); ++iter)
      iter->second.clear();
  }

  bool Vault_credentials_parser::is_valid_option(const Secure_string &option) const
  {
    return vault_credentials_in_progress.count(option) != 0;
  }

  bool Vault_credentials_parser::parse_line(uint line_number, const Secure_string& line, Vault_credentials *vault_credentials)
  {
    if (line.empty())
      return false;

    size_t eq_sign_pos = line.find('=');
    std::ostringstream err_ss;

    if (eq_sign_pos == std::string::npos)
    {
      err_ss << "Could not parse credential file. Cannot find equal sign (=) in line: ";
      err_ss << line_number << '.';
      logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
      return true;
    }
    Secure_string option = line.substr(0, eq_sign_pos);
    boost::trim(option); 

    if (!is_valid_option(option))
    {
      err_ss << "Could not parse credential file. Unknown option \"" << option << "\" in line: ";
      err_ss << line_number << '.';
      return true;
    }
    Secure_string *value = &(*vault_credentials)[option];

    if (!value->empty()) // repeated option in file
    {
      err_ss << "Could not parse credential file. Seems that value for option " << option;
      err_ss << " has been specified more than once in line: " << line_number << '.';
      logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
      return true;
    }
    *value = line.substr(eq_sign_pos + 1, line.size() - (eq_sign_pos + 1)); 
    boost::trim(*value);

    if (value->empty())
    {
      err_ss << "Could not parse credential file. Seems there is no value specified ";
      err_ss << "for option " << option << " in line: " << line_number << '.';

      logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
      return true;
    }
    return false;
  }

  bool Vault_credentials_parser::parse(const std::string &file_url, Vault_credentials *vault_credentials)
  {
    reset_vault_credentials(&vault_credentials_in_progress);

    File_io file_io(logger);
    File file = file_io.open(PSI_NOT_INSTRUMENTED,
                             file_url.c_str(),
                             O_RDONLY, MYF(MY_WME));

    if (file < 0)
    {
      logger->log(MY_ERROR_LEVEL, "Could not open file with credentials.");
      return true;
    }
    BOOST_SCOPE_EXIT(&file_io, &file)
    {
      file_io.close(file, MYF(MY_WME));
    } BOOST_SCOPE_EXIT_END

    if (file_io.seek(file, 0, MY_SEEK_END, MYF(MY_WME)) == MY_FILEPOS_ERROR)
    {
      logger->log(MY_ERROR_LEVEL, "Could not read file with credentials.");
      return true;
    }
    my_off_t file_size = file_io.tell(file, MYF(MY_WME));
    if (file_size == 0)
    {
      logger->log(MY_ERROR_LEVEL, "Empty file with credentials.");
      return true;
    }
    if (file_size > max_file_size ||
        file_io.seek(file, 0, MY_SEEK_SET, MYF(MY_WME)) == MY_FILEPOS_ERROR)
    {
      logger->log(MY_ERROR_LEVEL, "Could not read file with credentials.");
      return true;
    }
    boost::movelib::unique_ptr<uchar[]> buffer(new uchar[file_size]);
    if (file_io.read(file, buffer.get(), file_size, MYF(MY_WME)) != file_size)
    {
      logger->log(MY_ERROR_LEVEL, "Could not read file with credentials.");
      return true;
    }
    Secure_string file_content(reinterpret_cast<char*>(buffer.get()), file_size);
    Secure_istringstream credentials_ss(file_content);
    uint line_number = 1;
    Secure_string line;
    while (!getline(credentials_ss, line).fail())
      if (parse_line(line_number, line, &vault_credentials_in_progress))
      {
        line_number++;
        return true;
      }

    for (Vault_credentials::const_iterator iter = vault_credentials_in_progress.begin();
         iter != vault_credentials_in_progress.end(); ++iter)
    {
      if (iter->second.empty() && optional_value.count(iter->first) == 0)
      {
        std::ostringstream err_ss;
        err_ss << "Could not read " << iter->first << " from the configuration file.";
        logger->log(MY_ERROR_LEVEL, err_ss.str().c_str());
        return true;
      }
    }
    *vault_credentials = vault_credentials_in_progress;
    return false;
  }
} // namespace keyring
