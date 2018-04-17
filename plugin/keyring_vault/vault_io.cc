#include <my_global.h>
#include "vault_io.h"
#include "vault_keys_list.h"
#include "vault_credentials_parser.h"
#include <curl/curl.h>
#include <sstream>

namespace keyring {

my_bool Vault_io::init(std::string *keyring_storage_url)
{
  Vault_credentials_parser vault_credentials_parser(logger);
  Vault_credentials vault_credentials;
  return vault_credentials_parser.parse(*keyring_storage_url, &vault_credentials) ||
         vault_curl->init(vault_credentials);
}

Vault_io::~Vault_io()
{
  delete vault_curl;
  delete vault_parser;
}

Secure_string Vault_io::get_errors_from_response(const Secure_string &json_response)
{
  if (json_response.empty())
    return Secure_string();

  Secure_string errors_from_response, err_msg;
  if (vault_parser->parse_errors(json_response, &errors_from_response))
    err_msg = " Error while parsing error messages";
  else if (errors_from_response.size())
    err_msg = " Vault has returned the following error(s): " +
              errors_from_response;
  return err_msg;
}

my_bool Vault_io::get_serialized_object(ISerialized_object **serialized_object)
{
  static Secure_string err_msg("Could not retrieve list of keys from Vault.");
  *serialized_object= NULL;
  Secure_string json_response;

  if (vault_curl->list_keys(&json_response))
  {
    logger->log(MY_ERROR_LEVEL, (err_msg +
                get_errors_from_response(json_response)).c_str());
    return TRUE;
  }
  if (json_response.empty()) // no keys
  {
    *serialized_object = NULL;
    return FALSE;
  }

  std::auto_ptr<Vault_keys_list> keys(new Vault_keys_list());
  if (vault_parser->parse_keys(json_response, keys.get()))
  {
    logger->log(MY_ERROR_LEVEL, err_msg.c_str());
    return TRUE;
  }
  if (keys->size() == 0)
    keys.reset(NULL);

  *serialized_object = keys.release();
  return FALSE;
}

my_bool Vault_io::retrieve_key_type_and_data(IKey *key)
{
  Secure_string json_response;
  if(vault_curl->read_key(static_cast<const Vault_key&>(*key), &json_response) ||
     vault_parser->parse_key_data(json_response, key))
  {
    logger->log(MY_ERROR_LEVEL, ("Could not read key from Vault." +
                get_errors_from_response(json_response)).c_str());
    return TRUE;
  }
  return FALSE;
}

ISerializer* Vault_io::get_serializer()
{
  return &vault_key_serializer;
}

bool Vault_io::write_key(const Vault_key &key)
{
  Secure_string json_response, errors_from_response;
  if (vault_curl->write_key(key, &json_response) ||
      !((errors_from_response =
         get_errors_from_response(json_response)).empty()))
 {
    errors_from_response.insert(0, "Could not write key to Vault.");
    logger->log(MY_ERROR_LEVEL, errors_from_response.c_str());
    return true;
  }
  return false;
}

bool Vault_io::delete_key(const Vault_key &key)
{
  Secure_string json_response, errors_from_response;
  if(vault_curl->delete_key(key, &json_response) ||
     !((errors_from_response =
      get_errors_from_response(json_response)).empty()))
  {
    logger->log(MY_ERROR_LEVEL, ("Could not delete key from Vault." +
                errors_from_response).c_str());
    return true;
  }
  return false;
}

my_bool Vault_io::flush_to_storage(ISerialized_object *serialized_object)
{
  DBUG_ASSERT(serialized_object->has_next_key());
  IKey *vault_key_raw = NULL;

  if (serialized_object->get_next_key(&vault_key_raw) || vault_key_raw == NULL)
  {
    delete vault_key_raw;
    return TRUE;
  }
  std::auto_ptr<IKey> vault_key(vault_key_raw);

  return serialized_object->get_key_operation() == STORE_KEY
           ? write_key(static_cast<Vault_key&>(*vault_key))
           : delete_key(static_cast<Vault_key&>(*vault_key));
}

} // namespace keyring
