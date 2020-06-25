/*
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/server/builder/server_builder.h"

#include <memory>
#include <string>

#include "plugin/x/ngs/include/ngs/scheduler.h"
#include "plugin/x/ngs/include/ngs/socket_acceptors_task.h"
#include "plugin/x/ngs/include/ngs/socket_events.h"
#include "plugin/x/ngs/include/ngs/timeout_callback.h"
#include "plugin/x/src/server/scheduler_monitor.h"
#include "plugin/x/src/server/server.h"
#include "plugin/x/src/server/session_scheduler.h"
#include "plugin/x/src/variables/status_variables.h"
#include "plugin/x/src/variables/system_variables.h"
#include "plugin/x/src/xpl_log.h"

extern bool check_address_is_wildcard(const char *address_value,
                                      size_t address_length);

namespace xpl {

namespace details {

bool parse_bind_address_value(const char *begin_address_value,
                              std::string *address_value,
                              std::string *network_namespace) {
  const char *namespace_separator = strchr(begin_address_value, '/');

  if (namespace_separator != nullptr) {
    if (begin_address_value == namespace_separator)
      /*
        Parse error: there is no character before '/',
        that is missed address value
      */
      return true;

    if (*(namespace_separator + 1) == 0)
      /*
        Parse error: there is no character immediately after '/',
        that is missed namespace name.
      */
      return true;

    /*
      Found namespace delimiter. Extract namespace and address values
    */
    *address_value = std::string(begin_address_value, namespace_separator);
    *network_namespace = std::string(namespace_separator + 1);
  } else {
    *address_value = begin_address_value;
  }
  return false;
}

}  // namespace details

using Monitor_interface_ptr =
    std::unique_ptr<ngs::Scheduler_dynamic::Monitor_interface>;

Server_builder::Server_builder(MYSQL_PLUGIN plugin_handle)
    : m_events(ngs::allocate_shared<ngs::Socket_events>()),
      m_timeout_callback(ngs::allocate_shared<ngs::Timeout_callback>(m_events)),
      m_config(ngs::allocate_shared<ngs::Protocol_global_config>()),
      m_thd_scheduler(ngs::allocate_shared<xpl::Session_scheduler>(
          "work", plugin_handle,
          Monitor_interface_ptr{new xpl::Worker_scheduler_monitor()})) {}

std::shared_ptr<iface::Server_task> Server_builder::get_result_acceptor_task()
    const {
  uint32 listen_backlog =
      50 + xpl::Plugin_system_variables::m_max_connections / 5;
  if (listen_backlog > 900) listen_backlog = 900;

  std::string address_value, network_namespace;
  if (details::parse_bind_address_value(
          xpl::Plugin_system_variables::m_bind_address, &address_value,
          &network_namespace)) {
    log_error(ER_XPLUGIN_STARTUP_FAILED,
              "Invalid value for command line option mysqlx-bind-address");

    return {};
  }

  if (!network_namespace.empty() &&
      check_address_is_wildcard(address_value.c_str(),
                                address_value.length())) {
    log_error(ER_NETWORK_NAMESPACE_NOT_ALLOWED_FOR_WILDCARD_ADDRESS);
    log_error(ER_XPLUGIN_STARTUP_FAILED,
              "Invalid value for command line option mysqlx-bind-address");
    return {};
  }

  auto acceptors = ngs::allocate_shared<ngs::Socket_acceptors_task>(
      std::ref(m_listener_factory), address_value, network_namespace,
      xpl::Plugin_system_variables::m_port,
      xpl::Plugin_system_variables::m_port_open_timeout,
      xpl::Plugin_system_variables::m_socket, listen_backlog, m_events);

  return acceptors;
}

Plugin_system_variables::Value_changed_callback
Server_builder::get_result_reconfigure_server_callback() {
  auto thd_scheduler = m_thd_scheduler;
  auto config = m_config;

  return [thd_scheduler, config](THD *thd) {
    // Update came from THDVAR, we do not need it.
    if (nullptr != thd) return;

    const auto min = thd_scheduler->set_num_workers(
        xpl::Plugin_system_variables::m_min_worker_threads);
    if (min < xpl::Plugin_system_variables::m_min_worker_threads)
      xpl::Plugin_system_variables::m_min_worker_threads = min;

    thd_scheduler->set_idle_worker_timeout(
        xpl::Plugin_system_variables::m_idle_worker_thread_timeout * 1000);

    config->max_message_size =
        xpl::Plugin_system_variables::m_max_allowed_packet;
    config->connect_timeout =
        xpl::chrono::Seconds(xpl::Plugin_system_variables::m_connect_timeout);

    config->m_timeouts = xpl::Plugin_system_variables::get_global_timeouts();
  };
}

iface::Server *Server_builder::get_result_server_instance(
    const Server_task_vector &tasks) const {
  auto net_scheduler = ngs::allocate_shared<ngs::Scheduler_dynamic>(
      "network", KEY_thread_x_acceptor);
  auto result = ngs::allocate_object<ngs::Server>(
      net_scheduler, m_thd_scheduler, m_config,
      &xpl::Plugin_status_variables::m_properties, tasks, m_timeout_callback);

  return result;
}

}  // namespace xpl
