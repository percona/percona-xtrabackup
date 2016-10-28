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

#ifndef _SQL_DATA_CONTEXT_H_
#define _SQL_DATA_CONTEXT_H_

#include "ngs/protocol_encoder.h"

#include "mysql/service_my_snprintf.h"
#include "mysql/service_command.h"

#include "buffering_command_delegate.h"
#include "streaming_command_delegate.h"

// Same user account should be add to
// scripts/mysql_system_tables_data.sql
#define MYSQLXSYS_USER      "mysqlxsys"
#define MYSQLXSYS_HOST      "localhost"
#define MYSQLXSYS_ACCOUNT   MYSQLXSYS_USER "@" MYSQLXSYS_HOST


namespace ngs
{
  class IOptions_session;
  typedef boost::shared_ptr<IOptions_session> IOptions_session_ptr;
  class Protocol_encoder;
}  // namespace ngs


namespace xpl
{
typedef boost::function<bool (const std::string &password_hash)> On_user_password_hash;
typedef Buffering_command_delegate::Field_value Field_value;
typedef Buffering_command_delegate::Row_data    Row_data;

class Sql_data_context : private boost::noncopyable
{
public:
  struct Result_info
  {
    uint64_t affected_rows;
    uint64_t last_insert_id;
    uint32_t num_warnings;
    std::string message;
    uint32_t server_status;

    Result_info()
    : affected_rows(0),
      last_insert_id(0),
      num_warnings(0),
      server_status(0)
    {}
  };

  Sql_data_context(ngs::Protocol_encoder *proto, const bool query_without_authentication = false)
  : m_proto(proto), m_mysql_session(NULL),
    m_streaming_delegate(m_proto),
    m_user(NULL),
    m_host(NULL),
    m_ip(NULL),
    m_db(NULL),
    m_last_sql_errno(0),
    m_auth_ok(false),
    m_is_super(false),
    m_query_without_authentication(query_without_authentication),
    m_password_expired(false)
  {}

  virtual ~Sql_data_context();

  ngs::Error_code init();
  void deinit();

  ngs::Error_code init(const int client_port, const bool is_tls_activated);
  virtual ngs::Error_code authenticate(const char *user, const char *host, const char *ip, const char *db, On_user_password_hash password_hash_cb, bool allow_expired_passwords, ngs::IOptions_session_ptr &options_session);

  ngs::Protocol_encoder &proto()
  {
    DBUG_ASSERT(m_proto != NULL);
    return *m_proto;
  }

  MYSQL_SESSION mysql_session() const { return m_mysql_session; }

  uint64_t mysql_session_id() const;
  MYSQL_THD get_thd() const;

  void detach();

  ngs::Error_code set_connection_type(const bool is_tls_activated);
  bool kill();
  bool is_killed();
  bool is_acl_disabled();
  bool is_api_ready();
  bool wait_api_ready(boost::function<bool()> exiting);
  bool password_expired() const { return m_password_expired; }

  const char* authenticated_user() const { return m_user; }
  bool authenticated_user_is_super() const { return m_is_super; }
  void switch_to_local_user(const std::string &username);

  ngs::Error_code execute_kill_sql_session(uint64_t mysql_session_id);

  // can only be executed once authenticated
  virtual ngs::Error_code execute_sql_no_result(const std::string &sql, Result_info &r_info);
  virtual ngs::Error_code execute_sql_and_collect_results(const std::string &sql,
                                                          std::vector<Command_delegate::Field_type> &r_types,
                                                          Buffering_command_delegate::Resultset &r_rows,
                                                          Result_info &r_info);
  virtual ngs::Error_code execute_sql_and_process_results(const std::string &sql,
                                                          const Callback_command_delegate::Start_row_callback &start_row,
                                                          const Callback_command_delegate::End_row_callback &end_row,
                                                          Result_info &r_info);
  virtual ngs::Error_code execute_sql_and_stream_results(const std::string &sql, bool compact_metadata,
                                                         Result_info &r_info);

private:

  ngs::Error_code execute_sql(Command_delegate &deleg, const char *sql, size_t length, Result_info &r_info);

  ngs::Error_code switch_to_user(const char *username, const char *hostname, const char *address, const char *db);
  ngs::Error_code query_user(const char *user, const char *host, const char *ip, On_user_password_hash &hash_verification_cb, ngs::IOptions_session_ptr &options_session);

  static void default_completion_handler(void *ctx, unsigned int sql_errno, const char *err_msg);

  ngs::Protocol_encoder* m_proto;
  MYSQL_SESSION          m_mysql_session;

  Callback_command_delegate m_callback_delegate;
  Buffering_command_delegate m_buffering_delegate;
  Streaming_command_delegate m_streaming_delegate;

  char *m_user;
  char *m_host;
  char *m_ip;
  char *m_db;

  int m_last_sql_errno;
  std::string m_last_sql_error;

  bool m_auth_ok;
  bool m_is_super;
  bool m_query_without_authentication;
  bool m_password_expired;
};

} // namespace xpl

#undef MYSQL_CLIENT

#endif
