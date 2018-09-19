#include <memory>
#include "my_config.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/plugin_keyring.h"
#include "plugin/keyring/common/keyring.h"
#include "sql/sql_class.h"
#include "vault_io.h"
#include "vault_keys_container.h"
#include "vault_parser.h"

using keyring::IVault_curl;
using keyring::IVault_parser;
using keyring::Keys_iterator;
using keyring::Logger;
using keyring::Vault_curl;
using keyring::Vault_io;
using keyring::Vault_keys_container;
using keyring::Vault_parser;

mysql_rwlock_t LOCK_keyring;

static bool reset_curl() noexcept {
  curl_global_cleanup();
  return curl_global_init(CURL_GLOBAL_ALL) != 0;
}

static void handle_std_bad_alloc_exception(const std::string &message_prefix) {
  DBUG_ASSERT(0);
  const std::string error_message =
      message_prefix + " due to memory allocation failure";
  if (logger != nullptr) logger->log(MY_ERROR_LEVEL, error_message.c_str());
}

static void handle_unknown_exception(const std::string &message_prefix) {
  DBUG_ASSERT(0);
  const std::string error_message = message_prefix +
                                    " due to internal "
                                    "exception inside the keyring_vault plugin";
  if (logger != nullptr) logger->log(MY_ERROR_LEVEL, error_message.c_str());
}

static char *keyring_vault_config_file = nullptr;
static uint keyring_vault_timeout = 0;

int check_keyring_file_data(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                            SYS_VAR *var MY_ATTRIBUTE((unused)), void *save,
                            st_mysql_value *value) {
  char buff[FN_REFLEN + 1];
  const char *keyring_filename;
  int len = sizeof(buff);
  std::unique_ptr<IKeys_container> new_keys(
      new Vault_keys_container(logger.get()));

  (*(const char **)save) = nullptr;
  keyring_filename = value->val_str(value, buff, &len);
  if (keyring_filename == nullptr) return 1;
  PolyLock_rwlock keyring_rwlock(&LOCK_keyring);
  AutoWLock keyring_auto_wlokc(&keyring_rwlock);

  try {
    if (reset_curl()) {
      logger->log(MY_ERROR_LEVEL, "Cannot set keyring_vault_config_file");
      return 1;
    }
    std::unique_ptr<IVault_curl> vault_curl(
        new Vault_curl(logger.get(), keyring_vault_timeout));
    std::unique_ptr<IVault_parser> vault_parser(new Vault_parser(logger.get()));
    IKeyring_io *keyring_io(
        new Vault_io(logger.get(), vault_curl.get(), vault_parser.get()));
    vault_curl.release();
    vault_parser.release();
    if (new_keys->init(keyring_io, keyring_filename)) return 1;
    *reinterpret_cast<IKeys_container **>(save) = new_keys.release();
  } catch (const std::bad_alloc &e) {
    handle_std_bad_alloc_exception("Cannot set keyring_vault_config_file");
    return 1;
  } catch (...) {
    handle_unknown_exception("Cannot set keyring_vault_config_file");
    return 1;
  }
  return (0);
}

static MYSQL_SYSVAR_STR(
    config,                                             /* name       */
    keyring_vault_config_file,                          /* value      */
    PLUGIN_VAR_RQCMDARG,                                /* flags      */
    "The path to the keyring_vault configuration file", /* comment    */
    check_keyring_file_data,                            /* check()    */
    update_keyring_file_data,                           /* update()   */
    ""                                                  /* default    */
);

static void update_keyring_vault_timeout(MYSQL_THD thd MY_ATTRIBUTE((unused)),
                                         SYS_VAR *var MY_ATTRIBUTE((unused)),
                                         void *ptr, const void *val) noexcept {
  DBUG_ASSERT(dynamic_cast<Vault_keys_container *>(keys.get()) != nullptr);
  *reinterpret_cast<uint *>(ptr) = *reinterpret_cast<const uint *>(val);
  dynamic_cast<Vault_keys_container *>(keys.get())
      ->set_curl_timeout(*static_cast<const uint *>(val));
}

static MYSQL_SYSVAR_UINT(
    timeout,                                               /* name       */
    keyring_vault_timeout,                                 /* value      */
    PLUGIN_VAR_OPCMDARG,                                   /* flags      */
    "The keyring_vault - Vault server connection timeout", /* comment    */
    nullptr,                                               /* check()    */
    update_keyring_vault_timeout,                          /* update()   */
    15,                                                    /* default    */
    0,                                                     /* min        */
    86400,                                                 /* max - 24h  */
    0                                                      /* blocksize  */
);

static SYS_VAR *keyring_vault_system_variables[] = {
    MYSQL_SYSVAR(config), MYSQL_SYSVAR(timeout), nullptr};

static SERVICE_TYPE(registry) *reg_srv = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

static int keyring_vault_init(MYSQL_PLUGIN plugin_info MY_ATTRIBUTE((unused))) {
  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs)) return 1;
  try {
#ifdef HAVE_PSI_INTERFACE
    keyring_init_psi_keys();
#endif
    if (init_keyring_locks()) return 1;

    if (curl_global_init(CURL_GLOBAL_ALL) != 0) return 1;

    logger.reset(new Logger());
    keys.reset(new Vault_keys_container(logger.get()));
    std::unique_ptr<IVault_curl> vault_curl(
        new Vault_curl(logger.get(), keyring_vault_timeout));
    std::unique_ptr<IVault_parser> vault_parser(new Vault_parser(logger.get()));
    IKeyring_io *keyring_io =
        new Vault_io(logger.get(), vault_curl.get(), vault_parser.get());
    vault_curl.release();
    vault_parser.release();
    if (keys->init(keyring_io, keyring_vault_config_file)) {
      is_keys_container_initialized = false;
      logger->log(
          MY_ERROR_LEVEL,
          "keyring_vault initialization failure. Please check that"
          " the keyring_vault_config_file points to readable keyring_vault "
          "configuration"
          " file. Please also make sure Vault is running and accessible."
          " The keyring_vault will stay unusable until correct configuration "
          "file gets"
          " provided.");

      if (current_thd != nullptr)
        push_warning(current_thd, Sql_condition::SL_WARNING, 42000,
                     "keyring_vault initialization failure. Please check the "
                     "server log.");
      return 0;
    }
    is_keys_container_initialized = true;
    return 0;
  } catch (const std::bad_alloc &e) {
    handle_std_bad_alloc_exception("keyring_vault initialization failure");
    curl_global_cleanup();
    return 1;
  } catch (...) {
    handle_unknown_exception("keyring_vault initialization failure");
    curl_global_cleanup();
    return 1;
  }
}

int keyring_vault_deinit(void *arg MY_ATTRIBUTE((unused))) noexcept {
  keys.reset();
  logger.reset();
  keyring_file_data.reset();
  mysql_rwlock_destroy(&LOCK_keyring);
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);

  curl_global_cleanup();
  return 0;
}

bool mysql_key_fetch(const char *key_id, char **key_type, const char *user_id,
                     void **key, size_t *key_len) {
  return mysql_key_fetch<keyring::Vault_key>(key_id, key_type, user_id, key,
                                             key_len, "keyring_vault");
}

bool mysql_key_store(const char *key_id, const char *key_type,
                     const char *user_id, const void *key, size_t key_len) {
  return mysql_key_store<keyring::Vault_key>(key_id, key_type, user_id, key,
                                             key_len, "keyring_vault");
}

bool mysql_key_remove(const char *key_id, const char *user_id) {
  return mysql_key_remove<keyring::Vault_key>(key_id, user_id, "keyring_vault");
}

bool mysql_key_generate(const char *key_id, const char *key_type,
                        const char *user_id, size_t key_len) noexcept {
  try {
    std::unique_ptr<IKey> key_candidate(
        new keyring::Vault_key(key_id, key_type, user_id, NULL, 0));

    std::unique_ptr<uchar[]> key(new uchar[key_len]);
    if (key.get() == nullptr) return true;
    memset(key.get(), 0, key_len);
    if (!is_keys_container_initialized ||
        check_key_for_writing(key_candidate.get(), "generating") ||
        my_rand_buffer(key.get(), key_len))
      return true;

    return mysql_key_store(key_id, key_type, user_id, key.get(), key_len);
  } catch (const std::bad_alloc &e) {
    handle_std_bad_alloc_exception("Failed to generate a key");
    return true;
  } catch (...) {
    // We want to make sure that no exception leaves keyring_vault plugin and
    // goes into the server. That is why there are try..catch blocks in all
    // keyring_vault service methods.
    handle_unknown_exception("Failed to generate a key");
    return true;
  }
}

static void mysql_key_iterator_init(void **key_iterator) {
  *key_iterator = new Keys_iterator(logger.get());
  mysql_key_iterator_init<keyring::Vault_key>(
      static_cast<Keys_iterator *>(*key_iterator), "keyring_vault");
}

static void mysql_key_iterator_deinit(void *key_iterator) {
  mysql_key_iterator_deinit<keyring::Vault_key>(
      static_cast<Keys_iterator *>(key_iterator), "keyring_vault");
  delete static_cast<Keys_iterator *>(key_iterator);
}

static bool mysql_key_iterator_get_key(void *key_iterator, char *key_id,
                                       char *user_id) {
  return mysql_key_iterator_get_key<keyring::Vault_key>(
      static_cast<Keys_iterator *>(key_iterator), key_id, user_id,
      "keyring_vault");
}

/* Plugin type-specific descriptor */
static struct st_mysql_keyring keyring_descriptor = {
    MYSQL_KEYRING_INTERFACE_VERSION,
    mysql_key_store,
    mysql_key_fetch,
    mysql_key_remove,
    mysql_key_generate,
    mysql_key_iterator_init,
    mysql_key_iterator_deinit,
    mysql_key_iterator_get_key};

mysql_declare_plugin(keyring_vault){
    MYSQL_KEYRING_PLUGIN, /*   type                            */
    &keyring_descriptor,  /*   descriptor                      */
    "keyring_vault",      /*   name                            */
    "Percona",            /*   author                          */
    "store/fetch authentication data to/from Vault server", /*   description */
    PLUGIN_LICENSE_GPL,
    keyring_vault_init, /*   init function (when loaded)     */
    nullptr,
    keyring_vault_deinit,           /*   deinit function (when unloaded) */
    0x0100,                         /*   version                         */
    nullptr,                        /*   status variables                */
    keyring_vault_system_variables, /*   system variables                */
    nullptr,
    0,
} mysql_declare_plugin_end;
