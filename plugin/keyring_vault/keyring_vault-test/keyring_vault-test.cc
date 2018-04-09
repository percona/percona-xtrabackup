#include "../vault_keyring.cc"
#include "generate_credential_file.h"
#include "vault_keys_container_ex.h"
#include <iostream>
#include <my_global.h>
#include <set>
#include <sql_plugin_ref.h>
#include <time.h>

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
      : key_id(key_id), user_id(user_id),
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
    char key_id[12];   // Key#1000000\0
    char key_type[16]; // KeyType#1000000\0
    char user[13];     // User#1000000\0
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
    char key_id[12];   // Key#1000000\0
    char key_type[16]; // KeyType#1000000\0
    char user[13];     // User#1000000\0
    uchar key_stack[] = "KEeeeeeeeEEEEEeeeeEEEEEEEEEEEEY!";
    uchar *key;
    size_t key_len; // = rand() % 100;
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
    if (generate_random_keys_data)
      my_free(key);

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
    char key_id[12]; // Key#1000000\0
    char *key_type = NULL;
    char user[13]; // User#1000000\0
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
      if (key_type != NULL)
        std::cout << key_type << ' ';
      std::cout << user << ' ';
      std::cout << (result ? "successful" : "failed") << std::endl;
      mysql_mutex_unlock(&LOCK_verbose);
    }
    if (key_type != NULL)
      my_free(key_type);
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
    char key_id[12]; // Key#1000000\0
    char user[13];   // User#1000000\0

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
  if (init_keyring_locks())
    return true;

  if (init_curl())
    return true;

  st_plugin_int plugin_info;
  plugin_info.name.str = const_cast<char *>("keyring_vault");
  plugin_info.name.length = strlen("keyring_vault");

  logger.reset(new keyring::Logger(&plugin_info));
  // We are using Vault_keys_container_ex which allows removing all keys created
  // in keyring
  // Its behaviour is exactly the same as Vault_keys_container
  keys.reset(new keyring::Vault_keys_container_ex(logger.get()));
  boost::movelib::unique_ptr<IVault_curl> vault_curl(
      new Vault_curl(logger.get(), curl));
  boost::movelib::unique_ptr<IVault_parser> vault_parser(
      new Vault_parser(logger.get()));
  IKeyring_io *keyring_io =
      new Vault_io(logger.get(), vault_curl.release(), vault_parser.release());
  is_keys_container_initialized =
      !keys->init(keyring_io, keyring_vault_config_file);
  return !is_keys_container_initialized;
}

int main(int argc, char **argv) {
  my_thread_global_init();
  mysql_mutex_init(0, &LOCK_verbose, MY_MUTEX_INIT_FAST);
  mysql_mutex_init(0, &LOCK_keys_in_keyring, MY_MUTEX_INIT_FAST);

  keyring::system_charset_info = &my_charset_utf8_general_ci;
  srand(time(NULL));
  my_thread_handle *otid;
  unsigned long long i;
  void *tret;

  std::string credential_file_url = "./keyring_vault.conf";
  if (generate_credential_file(credential_file_url)) {
    std::cerr << "Could not generate default keyring configuration file" 
              << std::endl;
    return 1;
  }

  const char *default_args[] = {credential_file_url.c_str(),
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

  keyring_vault_config_file = static_cast<char *>(my_malloc(
      PSI_NOT_INSTRUMENTED,
      strlen(argument_passed ? argv[1] : default_args[0]) + 1, MYF(0)));
  strcpy(keyring_vault_config_file,
         (argument_passed ? argv[1] : default_args[0]));
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

  unsigned long long threads_number =
      threads_store_number + threads_fetch_number + threads_remove_number +
      threads_generate_key_number;

  if (!(otid = static_cast<my_thread_handle *>(my_malloc(
            PSI_NOT_INSTRUMENTED, threads_number * sizeof(*otid), MYF(0)))))
    return 7;

  if (keyring_vault_init_for_test()) {
    fprintf(stderr, "Could not initialize keyring_vault.");
    return 1;
  }

  for (i = 0; i < threads_store_number; i++)
    if (mysql_thread_create(PSI_NOT_INSTRUMENTED, &otid[i], NULL, store,
                            const_cast<int *>(&number_of_keys_to_store)))
      return 2;

  for (i = 0; i < threads_fetch_number; i++)
    if (mysql_thread_create(PSI_NOT_INSTRUMENTED,
                            &otid[threads_store_number + i], NULL, fetch,
                            const_cast<int *>(&number_of_keys_to_fetch)))
      return 3;

  for (i = 0; i < threads_remove_number; i++)
    if (mysql_thread_create(
            PSI_NOT_INSTRUMENTED,
            &otid[threads_store_number + threads_fetch_number + i], NULL,
            remove, const_cast<int *>(&number_of_keys_to_remove)))
      return 4;

  for (i = 0; i < threads_generate_key_number; i++)
    if (mysql_thread_create(PSI_NOT_INSTRUMENTED,
                            &otid[threads_store_number + threads_fetch_number +
                                  threads_remove_number + i],
                            NULL, generate,
                            static_cast<void *>(&number_of_keys_to_generate)))
      return 6;

  for (i = 0; i < threads_number; i++)
    my_thread_join(&otid[i], &tret);

  // To be sure that all keys added in this test are removed - we try to remove
  // all keys for which
  // store/generate functions returned success
  // keys_in_keyring is growing only set - as we do not want to add
  // synchronization over keyring service calls.
  for (std::set<Key_in_keyring>::const_iterator iter = keys_in_keyring.begin();
       iter != keys_in_keyring.end(); ++iter)
    mysql_key_remove(iter->key_id.c_str(), iter->user_id.c_str());

  my_free(keyring_vault_config_file);
  my_free(otid);
  mysql_mutex_destroy(&LOCK_verbose);
  mysql_mutex_destroy(&LOCK_keys_in_keyring);

  keyring_vault_deinit(NULL);
  my_end(0);

  return 0;
}
