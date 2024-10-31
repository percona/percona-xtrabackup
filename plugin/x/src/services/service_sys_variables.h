/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef PLUGIN_X_SRC_SERVICES_SERVICE_SYS_VARIABLES_H_
#define PLUGIN_X_SRC_SERVICES_SERVICE_SYS_VARIABLES_H_

#include "mysql/components/services/mysql_system_variable.h"
#include "plugin/x/src/interface/service_registry.h"
#include "plugin/x/src/interface/service_sys_variables.h"

namespace xpl {

class Service_sys_variables : public iface::Service_sys_variables {
 public:
  explicit Service_sys_variables(iface::Service_registry *registry);
  ~Service_sys_variables() override;

  bool get_variable(const char *component_name, const char *name, void **val,
                    size_t *out_length_of_val) override;

  bool is_valid() const override;

 private:
  iface::Service_registry *m_registry;
  SERVICE_TYPE_NO_CONST(mysql_system_variable_reader) * m_sys_variable;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SERVICES_SERVICE_SYS_VARIABLES_H_
