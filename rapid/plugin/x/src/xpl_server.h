/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _XPL_SERVER_H_
#define _XPL_SERVER_H_

#include <string>
#include <vector>
#include <boost/atomic.hpp>

#include "ngs/server.h"
#include "ngs/memory.h"
#include "ngs_common/connection_vio.h"
#include "xpl_client.h"
#include "xpl_session.h"
#include "mysql_show_variable_wrapper.h"
#include "xpl_global_status_variables.h"

#include <mysql/plugin.h>


namespace xpl
{

class Session;
class Sql_data_context;
class Server;
struct Ssl_config;

typedef boost::shared_ptr<Server> Server_ptr;

class Server : public ngs::Server_delegate
{
public:
  Server(my_socket tcp_socket,
         boost::shared_ptr<ngs::Scheduler_dynamic> wscheduler,
         boost::shared_ptr<ngs::Protocol_config> config);
  virtual ~Server();


  static int main(MYSQL_PLUGIN p);
  static int exit(MYSQL_PLUGIN p);

  template <void (Client::*method)(st_mysql_show_var *)>
  static void session_status_variable(THD *thd, st_mysql_show_var *var, char *buff);

  template <typename ReturnType, ReturnType (ngs::IOptions_session::*method)()>
  static void session_status_variable(THD *thd, st_mysql_show_var *var, char *buff);

  template <void (Server::*method)(st_mysql_show_var *)>
  static void global_status_variable(THD *thd, st_mysql_show_var *var, char *buff);

  template <typename ReturnType, ReturnType (xpl::Global_status_variables::*method)()>
  static void global_status_variable_server(THD *thd, st_mysql_show_var *var, char *buff);

  template <typename ReturnType, ReturnType (xpl::Common_status_variables::*method)() const>
  static void common_status_variable(THD *thd, st_mysql_show_var *var, char *buff);

  template <typename ReturnType, ReturnType (ngs::IOptions_context::*method)()>
  static void global_status_variable(THD *thd, st_mysql_show_var *var, char *buff);

  template<void (Common_status_variables::*method)()>
  static void update_status_variable(xpl::Common_status_variables &status_variables);

  ngs::Server &server() { return m_server; }

  ngs::Error_code kill_client(uint64_t client_id, Session &requester);

  typedef ngs::Locked_container<Server, ngs::RWLock_readlock, ngs::RWLock> Server_with_lock;
  typedef Memory_new<Server_with_lock>::Unique_ptr Server_ref;

  static Server_ref get_instance()
  {
    //TODO: ngs::Locked_container add container that supports shared_ptrs
    return instance ? Server_ref(new Server_with_lock(*instance, instance_rwl)) : Server_ref();
  }

private:
  static Client_ptr      get_client_by_thd(Server_ref &server, THD *thd);
  static void            create_mysqlx_user(Sql_data_context &context);
  static ngs::Error_code let_mysqlx_user_verify_itself(Sql_data_context &context);
  static void            verify_mysqlx_user_grants(Sql_data_context &context);

  bool on_net_startup();
  void on_net_shutdown();

  static void *net_thread(void *ctx);

  void start_verify_server_state_timer();
  bool on_verify_server_state();

  void plugin_system_variables_changed();

  virtual boost::shared_ptr<ngs::Client>  create_client(ngs::Connection_ptr connection);
  virtual boost::shared_ptr<ngs::Session> create_session(boost::shared_ptr<ngs::Client> client,
                                                         ngs::Protocol_encoder *proto,
                                                         ngs::Session::Session_id session_id);

  virtual bool will_accept_client(boost::shared_ptr<ngs::Client> client);
  virtual void did_accept_client(boost::shared_ptr<ngs::Client> client);
  virtual void did_reject_client(ngs::Server_delegate::Reject_reason reason);

  virtual void on_client_closed(boost::shared_ptr<ngs::Client> client);
  virtual bool is_terminating() const;

  static Server*      instance;
  static ngs::RWLock  instance_rwl;
  static MYSQL_PLUGIN plugin_ref;

  ngs::Thread_t               m_acceptor_thread;

  ngs::Client::Client_id      m_client_id;
  boost::atomics::atomic<int> m_num_of_connections;
  boost::shared_ptr<ngs::Protocol_config>   m_config;
  boost::shared_ptr<ngs::Scheduler_dynamic> m_wscheduler;
  ngs::Server                 m_server;

  static bool exiting;
  static bool is_exiting();
};

template <void (Client::*method)(st_mysql_show_var *)>
void Server::session_status_variable(THD *thd, st_mysql_show_var *var, char *buff)
{
  var->type= SHOW_UNDEF;
  var->value= buff;

  Server_ref server(get_instance());
  if (server)
  {
    boost::scoped_ptr<Mutex_lock> lock(new Mutex_lock((*server)->server().get_client_exit_mutex()));
    Client_ptr client = get_client_by_thd(server, thd);

    if (client)
      ((*client).*method)(var);
  }
}

template <typename ReturnType, ReturnType (ngs::IOptions_session::*method)()>
void Server::session_status_variable(THD *thd, st_mysql_show_var *var, char *buff)
{
  var->type= SHOW_UNDEF;
  var->value= buff;

  Server_ref server(get_instance());
  if (server)
  {
    boost::scoped_ptr<Mutex_lock> lock(new Mutex_lock((*server)->server().get_client_exit_mutex()));
    Client_ptr client = get_client_by_thd(server, thd);

    if (client)
    {
      ReturnType result = ((*client->connection().options()).*method)();
      mysqld::xpl_show_var(var).assign(result);
    }
  }
}

template <void (Server::*method)(st_mysql_show_var *)>
void Server::global_status_variable(THD *thd, st_mysql_show_var *var, char *buff)
{
  var->type= SHOW_UNDEF;
  var->value= buff;

  Server_ref server = get_instance();
  if (server)
  {
    Server* server_ptr = server->container();
    (server_ptr->*method)(var);
  }
}

template <typename ReturnType, ReturnType (xpl::Global_status_variables::*method)()>
void Server::global_status_variable_server(THD *thd, st_mysql_show_var *var, char *buff)
{
  var->type= SHOW_UNDEF;
  var->value= buff;

  ReturnType result = (Global_status_variables::instance().*method)();
  mysqld::xpl_show_var(var).assign(result);
}

template <typename ReturnType, ReturnType (xpl::Common_status_variables::*method)() const>
void Server::common_status_variable(THD *thd, st_mysql_show_var *var, char *buff)
{
  var->type = SHOW_UNDEF;
  var->value = buff;


  Server_ref server(get_instance());
  if (server)
  {
    boost::scoped_ptr<Mutex_lock> lock(new Mutex_lock((*server)->server().get_client_exit_mutex()));
    Client_ptr client = get_client_by_thd(server, thd);

    if (client)
    {
      boost::shared_ptr<xpl::Session> client_session(client->get_session());
      if (client_session)
      {
        Common_status_variables &common_status = client_session->get_status_variables();
        ReturnType result = (common_status.*method)();
        mysqld::xpl_show_var(var).assign(result);
      }
      return;
    }
  }

  Common_status_variables &common_status = Global_status_variables::instance();
  ReturnType result = (common_status.*method)();
  mysqld::xpl_show_var(var).assign(result);
}

template <typename ReturnType, ReturnType (ngs::IOptions_context::*method)()>
void Server::global_status_variable(THD *thd, st_mysql_show_var *var, char *buff)
{
  var->type= SHOW_UNDEF;
  var->value= buff;

  Server_ref server = get_instance();
  if (!server || !(*server)->server().ssl_context())
     return;
  ngs::IOptions_context_ptr context = (*server)->server().ssl_context()->options();
  if (!context)
    return;

  ReturnType result = ((*context).*method)();

  mysqld::xpl_show_var(var).assign(result);
}

template<void (Common_status_variables::*method)()>
void Server::update_status_variable(xpl::Common_status_variables &status_variables)
{
  (status_variables.*method)();

  Common_status_variables &common_status = Global_status_variables::instance();
  (common_status.*method)();
}


} // namespace xpl

#endif  // _XPL_SERVER_H_
