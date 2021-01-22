#include <ctime>
#include <iostream>
#include <memory>
#include <set>

#include <boost/scope_exit.hpp>

#include <my_atomic.h>
#include <my_global.h>
#include <my_rnd.h>

#include "sql_plugin_ref.h"

#include "../vault_keyring.cc"
#include "generate_credential_file.h"
#include "vault_keys_container_ex.h"
#include "vault_mount.h"

static bool random_keys = false;
static bool verbose;
static bool generate_random_keys_data = false;
static int number_of_keys_added = 0;
static int number_of_keys_fetched = 0;
static int number_of_keys_removed = 0;
static int number_of_keys_generated = 0;
static int max_generated_key_length = 0;
static int number_of_keys_to_generate = 0;

static mysql_mutex_t LOCK_verbose;
static mysql_mutex_t LOCK_keys_in_keyring;
struct Key_in_keyring {
  Key_in_keyring(const char *key_id, const char *user_id)
      : key_id(key_id),
        user_id(user_id),
        key_signature(this->key_id + this->user_id) {}

  Key_in_keyring() {}

  bool operator<(const Key_in_keyring &rhs_key) const {
    return this->key_signature < rhs_key.key_signature;
  }

  std::string key_id;
  std::string user_id;
  std::string key_signature;
};
static std::set<Key_in_keyring> keys_in_keyring;

static void *generate(void *arg) {
  my_thread_init();

  int number_of_keys_to_generate = *static_cast<int *>(arg);

  for (uint i = 0;
       my_atomic_load32(&number_of_keys_generated) < number_of_keys_to_generate;
       i = (i + 1) % number_of_keys_to_generate) {
    char key_id[12];    // Key#1000000\0
    char key_type[16];  // KeyType#1000000\0
    char user[13];      // User#1000000\0
    size_t key_len = rand() % 100;

    int key_nr = random_keys ? rand() % number_of_keys_to_generate : i;
    sprintf(key_id, "Key#%d", key_nr);
    strcpy(key_type, "AES");
    sprintf(user, "User#%d", key_nr);
    Key_in_keyring key_in_keyring(key_id, user);

    bool result = false;

    if ((result = !mysql_key_generate(reinterpret_cast<const char *>(key_id),
                                      reinterpret_cast<const char *>(key_type),
                                      reinterpret_cast<const char *>(user),
                                      key_len))) {
      my_atomic_add32(&number_of_keys_generated, 1);
      mysql_mutex_lock(&LOCK_keys_in_keyring);
      keys_in_keyring.insert(key_in_keyring);
      mysql_mutex_unlock(&LOCK_keys_in_keyring);
    }

    if (verbose) {
      mysql_mutex_lock(&LOCK_verbose);
      std::cout << "Key generated " << key_id << ' ' << key_type << ' ' << user
                << ' ';
      std::cout << (result ? "successful" : "failed") << std::endl;
      mysql_mutex_unlock(&LOCK_verbose);
    }
  }
  my_thread_end();
  return NULL;
}

static void *store(void *arg) {
  my_thread_init();
  int number_of_keys_to_store = *static_cast<int *>(arg);

  for (uint i = 0;
       my_atomic_load32(&number_of_keys_added) < number_of_keys_to_store;
       i = (i + 1) % number_of_keys_to_store) {
    char key_id[12];    // Key#1000000\0
    char key_type[16];  // KeyType#1000000\0
    char user[13];      // User#1000000\0
    uchar key_stack[] = "KEeeeeeeeEEEEEeeeeEEEEEEEEEEEEY!";
    uchar *key;
    size_t key_len;  // = rand() % 100;
    if (generate_random_keys_data) {
      key_len = rand() % max_generated_key_length;
      key = static_cast<uchar *>(
          my_malloc(keyring::key_memory_KEYRING, key_len, MYF(0)));
      assert(key != NULL);
      assert(!my_rand_buffer(key, key_len));
    } else {
      key = key_stack;
      key_len = strlen(reinterpret_cast<char *>(key)) + 1;
    }

    int key_nr = random_keys ? rand() % number_of_keys_to_store : i;
    sprintf(key_id, "Key#%d", key_nr);
    strcpy(key_type, "AES");
    sprintf(user, "User#%d", key_nr);
    Key_in_keyring key_in_keyring(key_id, user);

    bool result = false;

    if ((result = !mysql_key_store(reinterpret_cast<const char *>(key_id),
                                   reinterpret_cast<const char *>(key_type),
                                   reinterpret_cast<const char *>(user), key,
                                   key_len))) {
      my_atomic_add32(&number_of_keys_added, 1);
      mysql_mutex_lock(&LOCK_keys_in_keyring);
      keys_in_keyring.insert(key_in_keyring);
      mysql_mutex_unlock(&LOCK_keys_in_keyring);
    }
    if (generate_random_keys_data) my_free(key);

    if (verbose) {
      mysql_mutex_lock(&LOCK_verbose);
      std::cout << "Key stored " << key_id << ' ' << key_type << ' ' << user
                << ' ';
      std::cout << (result ? "successful" : "failed") << std::endl;
      mysql_mutex_unlock(&LOCK_verbose);
    }
  }
  my_thread_end();
  return NULL;
}

static void *fetch(void *arg) {
  my_thread_init();
  int number_of_keys_to_fetch = *(static_cast<int *>(arg));

  for (uint i = 0;
       my_atomic_load32(&number_of_keys_fetched) < number_of_keys_to_fetch;
       i = (i + 1) % number_of_keys_to_fetch) {
    char key_id[12];  // Key#1000000\0
    char *key_type = NULL;
    char user[13];  // User#1000000\0
    char key[] = "KEeeeeeeeEEEEEeeeeEEEEEEEEEEEEY!";

    int key_nr = random_keys ? rand() % number_of_keys_to_fetch : i;
    sprintf(key_id, "Key#%d", key_nr);
    sprintf(user, "User#%d", key_nr);

    void *key_data = NULL;
    size_t key_len = 0;

    bool result = true;

    if ((result = !mysql_key_fetch(
             reinterpret_cast<const char *>(key_id), &key_type,
             reinterpret_cast<const char *>(user), &key_data, &key_len)) &&
        key_data != NULL) {
      my_atomic_add32(&number_of_keys_fetched, 1);
      if (!generate_random_keys_data && number_of_keys_to_generate == 0) {
        assert(key_len == strlen(key) + 1);
        assert(strcmp(reinterpret_cast<const char *>(
                          reinterpret_cast<uchar *>(key_data)),
                      key) == 0);
      }
      my_free(key_data);
    }

    if (verbose) {
      mysql_mutex_lock(&LOCK_verbose);
      std::cout << "Key fetched " << key_id << ' ';
      if (key_type != NULL) std::cout << key_type << ' ';
      std::cout << user << ' ';
      std::cout << (result ? "successful" : "failed") << std::endl;
      mysql_mutex_unlock(&LOCK_verbose);
    }
    if (key_type != NULL) my_free(key_type);
  }
  my_thread_end();
  return NULL;
}

static void *remove(void *arg) {
  my_thread_init();
  int number_of_keys_to_remove = *(static_cast<int *>(arg));

  for (uint i = 0;
       my_atomic_load32(&number_of_keys_removed) < number_of_keys_to_remove;
       i = (i + 1) % number_of_keys_to_remove) {
    char key_id[12];  // Key#1000000\0
    char user[13];    // User#1000000\0

    int key_nr = random_keys ? rand() % number_of_keys_to_remove : i;
    sprintf(key_id, "Key#%d", key_nr);
    sprintf(user, "User#%d", key_nr);

    bool result = true;

    if ((result = !mysql_key_remove(reinterpret_cast<const char *>(key_id),
                                    reinterpret_cast<const char *>(user))))
      my_atomic_add32(&number_of_keys_removed, 1);

    if (verbose) {
      mysql_mutex_lock(&LOCK_verbose);
      std::cout << "Key removed " << key_id << ' ' << user << ' ';
      std::cout << (result ? "successful" : "failed") << std::endl;
      mysql_mutex_unlock(&LOCK_verbose);
    }
  }
  my_thread_end();
  return NULL;
}

static bool keyring_vault_init_for_test() {
#ifdef HAVE_PSI_INTERFACE
  keyring_init_psi_keys();
#endif
  if (init_keyring_locks()) return true;

  // We are using Vault_keys_container_ex which allows removing all keys created
  // in keyring
  // Its behaviour is exactly the same as Vault_keys_container
  keys.reset(new keyring::Vault_keys_container_ex(logger.get()));
  std::unique_ptr<IVault_parser_composer> vault_parser(
      new Vault_parser_composer(logger.get()));
  std::unique_ptr<IVault_curl> vault_curl(
      new Vault_curl(logger.get(), vault_parser.get(), 15));
  IKeyring_io *keyring_io =
      new Vault_io(logger.get(), vault_curl.release(), vault_parser.release());
  is_keys_container_initialized =
      !keys->init(keyring_io, keyring_vault_config_file);
  return !is_keys_container_initialized;
}

static char fake_plugin_name[] = "keyring_vault";

int main(int argc, char **argv) {
  if (!is_vault_environment_configured()) {
    std::cout << "[WARNING] Vault environment variables are not set. "
                 "Skipping the test."
              << std::endl;
    return 0;
  }

  typedef std::vector<my_thread_handle> my_thread_handle_container;
  my_thread_handle_container otid;
  std::size_t i;
  void *tret;

  std::string uuid = generate_uuid();
  std::string credential_file_name = "./keyring_vault" + uuid + ".conf";
  std::string mount_point = "mtr/" + uuid;
  if (generate_credential_file(
          credential_file_name, mount_point,
          mount_point_version_type::mount_point_version_v1,
          credentials_validity_type::credentials_validity_correct)) {
    std::cerr << "Could not generate default keyring configuration file"
              << std::endl;
    return 1;
  }
  BOOST_SCOPE_EXIT(&credential_file_name) {
    std::remove(credential_file_name.c_str());
  }
  BOOST_SCOPE_EXIT_END

  const char *default_args[] = {credential_file_name.c_str(),
                                "100",
                                "10",
                                "30",
                                "10",
                                "200",
                                "100",
                                "20",
                                "10",
                                "0",
                                "1"};
  bool argument_passed = argc == 12;

  if (!argument_passed) {
    std::cerr << "Usage: keyring_vault_test <path_to_keyring_vault_conf> "
                 "<threads_store_number> "
                 "<threads_remove_number> <threads_fetch_number> "
                 "<threads_generate_key_number> <number_of_keys_to_store> "
                 "<number_of_keys_to_fetch> <number_of_keys_to_generate> "
                 "<max_generated_key_length> <random_keys> <verbose>"
              << std::endl
              << "Using default values" << std::endl;
  }

  my_init();
  BOOST_SCOPE_EXIT(void) { my_end(0); }
  BOOST_SCOPE_EXIT_END

  mysql_mutex_init(0, &LOCK_verbose, MY_MUTEX_INIT_FAST);
  BOOST_SCOPE_EXIT(void) { mysql_mutex_destroy(&LOCK_verbose); }
  BOOST_SCOPE_EXIT_END

  mysql_mutex_init(0, &LOCK_keys_in_keyring, MY_MUTEX_INIT_FAST);
  BOOST_SCOPE_EXIT(void) { mysql_mutex_destroy(&LOCK_keys_in_keyring); }
  BOOST_SCOPE_EXIT_END

  system_charset_info = &my_charset_utf8_general_ci;
  srand(time(nullptr));

  typedef std::vector<char> char_container;
  char_container keyring_vault_config_file_storage;
  const char *credentials_file_name_raw =
      argument_passed ? argv[1] : default_args[0];
  keyring_vault_config_file_storage.assign(
      credentials_file_name_raw,
      credentials_file_name_raw + std::strlen(credentials_file_name_raw) + 1);

  keyring_vault_config_file = keyring_vault_config_file_storage.data();
  const unsigned long long threads_store_number =
      atoll(argument_passed ? argv[2] : default_args[1]);
  const unsigned long long threads_remove_number =
      atoll(argument_passed ? argv[3] : default_args[2]);
  const unsigned long long threads_fetch_number =
      atoll(argument_passed ? argv[4] : default_args[3]);
  const unsigned long long threads_generate_key_number =
      atoll(argument_passed ? argv[5] : default_args[4]);
  const int number_of_keys_to_store =
      atoi(argument_passed ? argv[6] : default_args[5]);
  const int number_of_keys_to_fetch =
      atoi(argument_passed ? argv[7] : default_args[6]);
  number_of_keys_to_generate =
      atoi(argument_passed ? argv[8] : default_args[7]);
  max_generated_key_length = atoi(argument_passed ? argv[9] : default_args[8]);
  random_keys = atoi(argument_passed ? argv[10] : default_args[9]);
  verbose = atoi(argument_passed ? argv[11] : default_args[10]);
  const int number_of_keys_to_remove =
      number_of_keys_to_store + number_of_keys_to_generate;

  st_plugin_int plugin_info;
  plugin_info.name.str = fake_plugin_name;
  plugin_info.name.length = sizeof(fake_plugin_name) - 1;
  logger.reset(new keyring::Logger(&plugin_info));

  curl_global_init(CURL_GLOBAL_DEFAULT);
  BOOST_SCOPE_EXIT(void) { curl_global_cleanup(); }
  BOOST_SCOPE_EXIT_END

  CURL *curl = curl_easy_init();
  if (curl == nullptr) {
    std::cerr << "Could not initialize CURL session" << std::endl;
    return 1;
  }
  BOOST_SCOPE_EXIT(&curl) { curl_easy_cleanup(curl); }
  BOOST_SCOPE_EXIT_END

  // create unique secret mount point for this test suite
  keyring::Vault_mount vault_mount(curl, logger.get());
  if (vault_mount.init(credential_file_name, mount_point)) {
    std::cout << "Could not initialize Vault_mount" << std::endl;
    return 2;
  }
  if (vault_mount.mount_secret_backend()) {
    std::cout << "Could not mount secret backend" << std::endl;
    return 3;
  }
  BOOST_SCOPE_EXIT(&vault_mount) { vault_mount.unmount_secret_backend(); }
  BOOST_SCOPE_EXIT_END

  if (keyring_vault_init_for_test()) {
    std::cerr << "Could not initialize keyring_vault." << std::endl;
    return 4;
  }
  BOOST_SCOPE_EXIT(void) { keyring_vault_deinit(NULL); }
  BOOST_SCOPE_EXIT_END

  unsigned long long threads_number =
      threads_store_number + threads_fetch_number + threads_remove_number +
      threads_generate_key_number;

  BOOST_SCOPE_EXIT(void) {
    // To be sure that all keys added in this test are removed - we try to
    // remove all keys for which store/generate functions returned success
    // keys_in_keyring is growing only set - as we do not want to add
    // synchronization over keyring service calls.
    for (auto &key : keys_in_keyring)
      mysql_key_remove(key.key_id.c_str(), key.user_id.c_str());
  }
  BOOST_SCOPE_EXIT_END

  my_thread_handle current_thread_handle;
  current_thread_handle.thread = my_thread_self();
  otid.resize(threads_number, current_thread_handle);
  BOOST_SCOPE_EXIT((&otid)(&threads_number)(&tret)(&current_thread_handle)) {
    for (std::size_t u = 0; u < threads_number; ++u)
      if (!my_thread_equal(otid[u].thread, current_thread_handle.thread))
        my_thread_join(&otid[u], &tret);
  }
  BOOST_SCOPE_EXIT_END

  for (i = 0; i < threads_store_number; i++)
    if (mysql_thread_create(PSI_NOT_INSTRUMENTED, &otid[i], NULL, store,
                            const_cast<int *>(&number_of_keys_to_store)))
      return 5;

  for (i = 0; i < threads_fetch_number; i++)
    if (mysql_thread_create(PSI_NOT_INSTRUMENTED,
                            &otid[threads_store_number + i], NULL, fetch,
                            const_cast<int *>(&number_of_keys_to_fetch)))
      return 6;

  for (i = 0; i < threads_remove_number; i++)
    if (mysql_thread_create(
            PSI_NOT_INSTRUMENTED,
            &otid[threads_store_number + threads_fetch_number + i], NULL,
            remove, const_cast<int *>(&number_of_keys_to_remove)))
      return 7;

  for (i = 0; i < threads_generate_key_number; i++)
    if (mysql_thread_create(PSI_NOT_INSTRUMENTED,
                            &otid[threads_store_number + threads_fetch_number +
                                  threads_remove_number + i],
                            nullptr, generate,
                            static_cast<void *>(&number_of_keys_to_generate)))
      return 8;

  return 0;
}
