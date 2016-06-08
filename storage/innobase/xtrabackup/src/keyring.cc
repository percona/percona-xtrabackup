/******************************************************
Copyright (c) 2016 Percona LLC and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*******************************************************/

#include <my_base.h>
#include <my_aes.h>
#include <../plugin/keyring/keyring.h>
#include "common.h"

class XtraKLogger : public ILogger
{
public:
	XtraKLogger()
	{}
	~XtraKLogger() {}
	void log(plugin_log_level level, const char *message)
	{
		msg("%s\n", message);
	}
};


bool
xb_keyring_init(const char *file_path)
{
	const char *keyring_file_data_value = file_path;
	MY_STAT stat_arg;

	try {
		logger.reset(new XtraKLogger());

		if (file_path == NULL) {
			return(false);
		}

		if (!my_stat(file_path, &stat_arg, MYF(0))) {
			logger->log(MY_ERROR_LEVEL, "Could not find keyring file.");
			return(false);
		}

		if (stat_arg.st_size == 0) {
			logger->log(MY_ERROR_LEVEL, "Keyring file is empty.");
			return(false);
		}

		if (init_keyring_locks())
			return(false);

		if (create_keyring_dir_if_does_not_exist(
			keyring_file_data_value))
		{
			logger->log(MY_ERROR_LEVEL, "Could not create keyring "
				"directory. The keyring_file will stay "
				"unusable until correct path to the keyring "
				"directory gets provided");
			return(false);
		}
		Buffered_file_io keyring_io(logger.get());
		keys.reset(new Keys_container(logger.get()));
		if (keys->init(&keyring_io, keyring_file_data_value))
		{
			is_keys_container_initialized = FALSE;
			logger->log(MY_ERROR_LEVEL, "keyring_file "
				"initialization failure. Please check "
				"if the keyring_file_data points to readable "
				"keyring file or keyring file "
				"can be created in the specified location. "
				"The keyring_file will stay unusable until "
				"correct path to the keyring file "
				"gets provided");
			return(false);
		}
		is_keys_container_initialized = TRUE;
		return(true);
	}
	catch (...)
	{
		return(false);
	}
}

int my_key_fetch(const char *key_id, char **key_type, const char *user_id,
                 void **key, size_t *key_len)
{
  try
  {
    return mysql_key_fetch(key_id, key_type, user_id, key,
                           key_len);
  }
  catch (...)
  {
    return TRUE;
  }
}

int my_key_store(const char *key_id, const char *key_type, const char *user_id,
                 const void *key, size_t key_len)
{
  try
  {
    Buffered_file_io keyring_io(logger.get());
    return mysql_key_store(&keyring_io, key_id, key_type, user_id, key,
                           key_len);
  }
  catch (...)
  {
    return TRUE;
  }
}

int my_key_remove(const char *key_id, const char *user_id)
{
  try
  {
    Buffered_file_io keyring_io(logger.get());
    return mysql_key_remove(&keyring_io, key_id, user_id);
  }
  catch (...)
  {
    return TRUE;
  }
}

int my_key_generate(const char *key_id, const char *key_type,
                    const char *user_id, size_t key_len)
{
  try
  {
    Buffered_file_io keyring_io(logger.get());
    return mysql_key_generate(&keyring_io, key_id, key_type, user_id, key_len);
  }
  catch (...)
  {
    return TRUE;
  }
}
