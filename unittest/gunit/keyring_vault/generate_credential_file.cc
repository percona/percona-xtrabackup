#include "generate_credential_file.h"

#include <cstdlib>
#include <fstream>
#include <ostream>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace {
const char vault_address_env_var_name[] = "MTR_VAULT_ADDRESS";
const char vault_token_env_var_name[] = "MTR_VAULT_TOKEN";
const char vault_ca_env_var_name[] = "MTR_VAULT_CA";
}  // anonymous namespace

std::ostream &operator<<(std::ostream &os,
                         mount_point_version_type mount_point_version) {
  const char *label = "";
  switch (mount_point_version) {
    case mount_point_version_type::mount_point_version_empty:
      label = "<EMPTY>";
      break;
    case mount_point_version_type::mount_point_version_v1:
      label = "1";
      break;
    case mount_point_version_type::mount_point_version_v2:
      label = "2";
      break;
    case mount_point_version_type::mount_point_version_auto:
      label = "AUTO";
      break;
  }
  return os << label;
}

std::string generate_uuid() {
  return boost::uuids::to_string(boost::uuids::random_generator()());
}

bool generate_credential_file(const std::string &credential_file_path,
                              const std::string &secret_mount_point,
                              mount_point_version_type mount_point_version,
                              credentials_validity_type generate_credetials) {
  std::remove(credential_file_path.c_str());

  const char *imported_vault_conf_address =
      std::getenv(vault_address_env_var_name);
  if (imported_vault_conf_address == nullptr) return true;

  const char *imported_vault_conf_token = std::getenv(vault_token_env_var_name);
  if (imported_vault_conf_token == nullptr) return true;
  const char *imported_vault_conf_ca = std::getenv(vault_ca_env_var_name);

  std::ofstream credentials_ofs(credential_file_path.c_str());
  if (!credentials_ofs.is_open()) return true;

  credentials_ofs << "vault_url = " << imported_vault_conf_address << '\n';
  credentials_ofs << "secret_mount_point = " << secret_mount_point << '\n';
  credentials_ofs
      << "token = "
      << (generate_credetials ==
                  credentials_validity_type::credentials_validity_invalid_token
              ? "token = 123-123-123"
              : imported_vault_conf_token)
      << '\n';
  if (imported_vault_conf_ca != NULL) {
    credentials_ofs << "vault_ca = " << imported_vault_conf_ca << '\n';
  }
  if (mount_point_version !=
      mount_point_version_type::mount_point_version_empty) {
    credentials_ofs << "secret_mount_point_version = " << mount_point_version
                    << '\n';
  }

  return false;
}

bool is_vault_environment_configured() {
  return std::getenv(vault_address_env_var_name) != nullptr &&
         std::getenv(vault_token_env_var_name) != nullptr;
}
