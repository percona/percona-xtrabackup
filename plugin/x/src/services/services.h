/* Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is also distributed with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with MySQL.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PLUGIN_X_SRC_SERVICES_SERVICES_H_
#define PLUGIN_X_SRC_SERVICES_SERVICES_H_

#include <memory>

#include "plugin/x/src/interface/service_sys_variables.h"
#include "plugin/x/src/services/service_registry.h"

namespace xpl {

class Services {
 public:
  using Sys_variables_ptr = std::unique_ptr<iface::Service_sys_variables>;

 public:
  Services();

  bool is_valid() const { return m_system_variable_register->is_valid(); }

  Service_registry m_registry;
  Sys_variables_ptr m_system_variable_register;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SERVICES_SERVICES_H_
