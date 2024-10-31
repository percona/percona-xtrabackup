/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <components/keyrings/common/component_helpers/include/service_requirements.h>
#include <components/keyrings/common/operations/operations.h>
#include <mysql/components/services/log_builtins.h> /* LogComponentErr */
#include "mysql/components/component_implementation.h"
#include "mysql/components/services/registry.h"
#include "mysqld_error.h" /* Errors */

#include "backend/backend.h"
#include "config/config.h"

namespace keyring_file {
/** Keyring operations object */
extern keyring_common::operations::Keyring_operations<
    backend::Keyring_file_backend> *g_keyring_operations;

/** Component callbacks */
extern keyring_common::service_implementation::Component_callbacks
    *g_component_callbacks;

/** Keyring data source */
extern config::Config_pod *g_config_pod;

/** Keyring state */
extern bool g_keyring_file_inited;

/* Initialize keyring */
bool init_or_reinit_keyring(std::string &err);

bool set_paths(const char *component_path, const char *instance_path);

}  // namespace keyring_file

extern REQUIRES_SERVICE_PLACEHOLDER(registry);
extern REQUIRES_SERVICE_PLACEHOLDER(registry_registration);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(registry,
                                       mysql_service_registry_no_lock);
extern REQUIRES_SERVICE_PLACEHOLDER_AS(registry_registration,
                                       mysql_service_registration_no_lock);