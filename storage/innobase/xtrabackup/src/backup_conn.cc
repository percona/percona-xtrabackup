/******************************************************
Copyright (c) 2026 Percona LLC and/or its affiliates.

Connections to a MySQL server: where they go, what they are for, and how long
they live.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA

*******************************************************/

#include <fil0fil.h>
#include <ha_prototypes.h>
#include <my_sys.h>
#include <mysql.h>
#include <string.h>

#include <limits>

#include "backup_conn.h"
#include "common.h"
#include "xb0xb.h"
#include "xtrabackup.h"

namespace {

/** The connection the backup runs on. */
xb::Connection main_connection;

/** The connection to the server that keeps the history record. */
xb::Connection history_connection;

}  // namespace

xb::Connection &main_conn() { return (main_connection); }

xb::Connection &history_conn() {
  return (history_connection.is_open() ? history_connection : main_connection);
}

namespace xb {

void Connection::close() {
  if (m_mysql != nullptr) {
    mysql_close(m_mysql);
    m_mysql = nullptr;
  }
}

/*********************************************************************/ /**
 Name a purpose for the log.
 @param[in]	purpose	what a connection is for
 @return name of the purpose */
static const char *purpose_name(Purpose purpose) {
  switch (purpose) {
    case Purpose::BACKUP:
      return ("backup");
    case Purpose::MDL_LOCK:
      return ("MDL lock");
    case Purpose::QUERY_KILLER:
      return ("query killer");
    case Purpose::REDO_ARCHIVE:
      return ("redo log archive");
    case Purpose::REDO_CONSUMER:
      return ("redo log consumer");
    case Purpose::HISTORY_RECORD:
      return ("history record");
  }

  ut_error;
}

/** The options that describe where the history record is kept. Naming any of
them is what asks for a connection of its own. */
static const char *history_dsn_options[] = {"history-host", "history-port",
                                            "history-socket", "history-user",
                                            "history-password"};

/** The options that describe how the history connection is secured. They are
inherited from --ssl-* as a group: naming one of them replaces all five. */
static const char *history_tls_options[] = {
    "history-ssl-mode", "history-ssl-ca", "history-ssl-capath",
    "history-ssl-cert", "history-ssl-key"};

/*********************************************************************/ /**
 Whether any of a set of options was named, on the command line or in a
 defaults file.
 @param[in]	names	option names, spelled as the option table spells them
 @return true if at least one of them was named */
template <size_t N>
static bool any_param_set(const char *const (&names)[N]) {
  for (const char *name : names) {
    if (check_if_param_set(name)) {
      return (true);
    }
  }

  return (false);
}

int Dsn::apply_tls(MYSQL *mysql) const {
  int result = set_client_ssl_options(mysql);

  if (!tls.present) {
    return (result);
  }

  /* Naming a CA is only meaningful once the certificate is actually verified,
  which is how set_client_ssl_options() treats it too. */
  if (tls.mode >= SSL_MODE_VERIFY_CA) {
    mysql_options(mysql, MYSQL_OPT_SSL_CA, tls.ca);
    mysql_options(mysql, MYSQL_OPT_SSL_CAPATH, tls.capath);
  } else {
    mysql_options(mysql, MYSQL_OPT_SSL_CA, nullptr);
    mysql_options(mysql, MYSQL_OPT_SSL_CAPATH, nullptr);
  }

  mysql_options(mysql, MYSQL_OPT_SSL_CERT, tls.cert);
  mysql_options(mysql, MYSQL_OPT_SSL_KEY, tls.key);
  mysql_options(mysql, MYSQL_OPT_SSL_MODE, &tls.mode);

  return (result);
}

bool is_configured(Destination destination) {
  if (destination == Destination::MAIN) {
    return (true);
  }

  return (any_param_set(history_dsn_options) ||
          any_param_set(history_tls_options));
}

static Dsn resolve(Destination destination) {
  Dsn dsn{opt_host, opt_user, opt_password, opt_port, opt_socket, {}};

  if (destination == Destination::MAIN) {
    return (dsn);
  }

  /* The history destination starts from the backup one and takes whatever was
  named for it. */
  if (check_if_param_set("history-host")) {
    dsn.host = opt_history_host;
    /* The socket belongs to the server being backed up, so it is not carried
    over to another host. */
    dsn.socket = nullptr;
  }
  if (check_if_param_set("history-port")) {
    dsn.port = opt_history_port;
  }
  if (check_if_param_set("history-socket")) {
    dsn.socket = opt_history_socket;
  }
  if (check_if_param_set("history-user")) {
    dsn.user = opt_history_user;
    /* The password belongs to the backup account, so it is inherited only
    together with the user it authenticates. */
    dsn.password = nullptr;
  }
  if (check_if_param_set("history-password")) {
    dsn.password = opt_history_password;
  }

  if (any_param_set(history_tls_options)) {
    dsn.tls.present = true;
    dsn.tls.ca = opt_history_ssl_ca;
    dsn.tls.capath = opt_history_ssl_capath;
    dsn.tls.cert = opt_history_ssl_cert;
    dsn.tls.key = opt_history_ssl_key;

    if (check_if_param_set("history-ssl-mode")) {
      dsn.tls.mode = opt_history_ssl_mode;
    } else if (opt_history_ssl_ca != nullptr ||
               opt_history_ssl_capath != nullptr) {
      /* Verify the history server against the CA that was named for it, the
      way --ssl-ca does for the backup server. */
      dsn.tls.mode = SSL_MODE_VERIFY_CA;
    }
  }

  return (dsn);
}

bool validate_connection_options() {
  if (!xtrabackup_backup || !is_configured(Destination::HISTORY)) {
    return (true);
  }

  if (opt_history == nullptr && opt_incremental_history_name == nullptr &&
      opt_incremental_history_uuid == nullptr) {
    xb::error() << "The options describing the connection to the history "
                   "server require --history, --incremental-history-name or "
                   "--incremental-history-uuid to be set.";
    return (false);
  }

  if (check_if_param_set("history-ssl-mode") &&
      opt_history_ssl_mode < SSL_MODE_VERIFY_CA &&
      (opt_history_ssl_ca != nullptr || opt_history_ssl_capath != nullptr)) {
    xb::warn() << "--history-ssl-ca and --history-ssl-capath have no effect "
                  "because --history-ssl-mode is below VERIFY_CA. The "
                  "certificate of the history server will not be verified.";
  }

  return (true);
}

/*********************************************************************/ /**
 The server a purpose is served by.

 Storing the history record is the only thing the history server is asked to
 do. Every other purpose belongs to the server being backed up and would be
 actively wrong anywhere else: the MDL lock and the query killer would take
 hold of, and interfere with, a server that is not being backed up at all, and
 the redo log purposes describe a copy that is not being made there. None of
 that would be reported either, since the connection would open and the
 statements would succeed.

 Answering it here rather than at the call sites is what makes those pairings
 impossible to write. The switch has no default, so a purpose added later does
 not compile until it says where it belongs.
 @param[in]	purpose	what a connection is for
 @return the server that serves it */
static Destination destination_of(Purpose purpose) {
  switch (purpose) {
    case Purpose::BACKUP:
    case Purpose::MDL_LOCK:
    case Purpose::QUERY_KILLER:
    case Purpose::REDO_ARCHIVE:
    case Purpose::REDO_CONSUMER:
      return (Destination::MAIN);
    case Purpose::HISTORY_RECORD:
      return (Destination::HISTORY);
  }

  ut_error;
}

bool connect(Purpose purpose, Connection &connection) {
  const Destination destination = destination_of(purpose);
  const Dsn dsn = resolve(destination);
  char mysql_port_str[std::numeric_limits<int>::digits10 + 3];

  /* Opening over a connection that is already open would close it, and with it
  whatever that session holds. */
  ut_a(!connection.is_open());

  connection.m_destination = destination;
  connection.m_purpose = purpose;

  snprintf(mysql_port_str, sizeof(mysql_port_str), "%u", dsn.port);

  /* The connection owns the handle from the moment it exists, so that it is
  closed in one place however the rest of this function ends. */
  connection.m_mysql = mysql_init(NULL);

  if (!connection.is_open()) {
    xb::error() << "Failed to init MySQL struct.";
    return (false);
  }

  xb::info() << "Connecting to MySQL server host: "
             << (dsn.host ? dsn.host : "localhost")
             << ", user: " << (dsn.user ? dsn.user : "not set")
             << ", password: " << (dsn.password ? "set" : "not set")
             << ", port: " << (dsn.port != 0 ? mysql_port_str : "not set")
             << ", socket: " << (dsn.socket ? dsn.socket : "not set")
             << ", purpose: " << purpose_name(purpose);

  if (dsn.apply_tls(connection) != 0) {
    xb::warn() << "Failed to set ssl related options.";
    if (mysql_errno(connection) != 0) {
      xb::warn() << mysql_error(connection);
    }
  }

  if (!mysql_real_connect(connection, dsn.host ? dsn.host : "localhost",
                          dsn.user, dsn.password, "" /*database*/, dsn.port,
                          dsn.socket, 0)) {
    xb::error() << "Failed to connect to MySQL server: "
                << mysql_error(connection);
    connection.close();
    return (false);
  }

  xb_mysql_query(connection, "SET SESSION wait_timeout=2147483", false, true);

  if (xb_mysql_numrows(connection,
                       "SHOW GLOBAL VARIABLES LIKE 'wsrep_sync_wait'",
                       false) > 0) {
    xb_mysql_query(connection, "SET SESSION wsrep_sync_wait=0", false, true);
  }

  if (xb_mysql_numrows(
          connection,
          "SELECT * FROM performance_schema.replication_group_members",
          false) > 0) {
    xb_mysql_query(connection,
                   "SET SESSION group_replication_consistency=EVENTUAL", false,
                   true);
  }

  xb_mysql_query(connection, "SET SESSION autocommit=1", false, true);

  xb_mysql_query(connection, "SET NAMES utf8", false, true);

  return (true);
}

bool open_connections() {
  if (!connect(Purpose::BACKUP, main_connection)) {
    return (false);
  }

  /* Opened here rather than when the record is read or written, so that a
  history server that cannot be reached, or an account that cannot log in to
  it, fails the backup before any data is copied instead of after it has all
  been written. */
  if (is_configured(Destination::HISTORY) &&
      !connect(Purpose::HISTORY_RECORD, history_connection)) {
    return (false);
  }

  return (true);
}

void close_connections() {
  history_connection.close();
  main_connection.close();
}

const std::list<std::string> &Connection::granted_privileges() {
  if (!m_granted_privileges) {
    std::list<std::string> privileges;
    MYSQL_RES *result = xb_mysql_query(m_mysql, "SHOW GRANTS", true);
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(result))) {
      privileges.push_back(*row);
    }
    mysql_free_result(result);

    m_granted_privileges = std::move(privileges);
  }

  return (*m_granted_privileges);
}

}  // namespace xb

/*********************************************************************/ /**
 Execute mysql query. */
MYSQL_RES *xb_mysql_query(MYSQL *connection, const char *query, bool use_result,
                          bool die_on_error) {
  MYSQL_RES *mysql_result = NULL;

  if (mysql_query(connection, query)) {
    xb::error() << "failed to execute query " << SQUOTE(query) << " : "
                << mysql_errno(connection) << " ("
                << mysql_errno_to_sqlstate(mysql_errno(connection)) << ") "
                << mysql_error(connection);
    if (die_on_error) {
      exit(EXIT_FAILURE);
    }
    return (NULL);
  }

  /* store result set on client if there is a result */
  if (mysql_field_count(connection) > 0) {
    if ((mysql_result = mysql_store_result(connection)) == NULL) {
      xb::error() << "failed to fetch query result " << query << " : "
                  << mysql_error(connection);
      if (die_on_error) {
        exit(EXIT_FAILURE);
      }
    }

    if (!use_result) {
      mysql_free_result(mysql_result);
    }
  }

  return mysql_result;
}

my_ulonglong xb_mysql_numrows(MYSQL *connection, const char *query,
                              bool die_on_error) {
  my_ulonglong rows_count = 0;
  MYSQL_RES *result = xb_mysql_query(connection, query, true, die_on_error);
  if (result) {
    rows_count = mysql_num_rows(result);
    mysql_free_result(result);
  }
  return rows_count;
}

/*********************************************************************/ /**
 Read mysql_variable from MYSQL_RES, return number of rows consumed. */
int read_mysql_variables_from_result(MYSQL_RES *mysql_result,
                                     mysql_variable *vars,
                                     bool vertical_result) {
  MYSQL_ROW row;
  mysql_variable *var;
  ut_ad(!vertical_result || mysql_num_fields(mysql_result) == 2);
  int rows_read = 0;

  if (vertical_result) {
    while ((row = mysql_fetch_row(mysql_result))) {
      ++rows_read;
      char *name = row[0];
      char *value = row[1];
      for (var = vars; var->name; var++) {
        if (strcasecmp(var->name, name) == 0 && value != NULL) {
          *(var->value) = strdup(value);
        }
      }
    }
  } else {
    MYSQL_FIELD *field;

    if ((row = mysql_fetch_row(mysql_result)) != NULL) {
      mysql_field_seek(mysql_result, 0);
      ++rows_read;
      int i = 0;
      while ((field = mysql_fetch_field(mysql_result)) != NULL) {
        char *name = field->name;
        char *value = row[i];
        for (var = vars; var->name; var++) {
          if (strcasecmp(var->name, name) == 0 && value != NULL) {
            *(var->value) = strdup(value);
          }
        }
        ++i;
      }
    }
  }
  return rows_read;
}

void read_mysql_variables(MYSQL *connection, const char *query,
                          mysql_variable *vars, bool vertical_result) {
  MYSQL_RES *mysql_result = xb_mysql_query(connection, query, true);
  read_mysql_variables_from_result(mysql_result, vars, vertical_result);
  mysql_free_result(mysql_result);
}

void free_mysql_variables(mysql_variable *vars) {
  mysql_variable *var;

  for (var = vars; var->name; var++) {
    free(*(var->value));
    *var->value = NULL;
  }
}

char *read_mysql_one_value(MYSQL *connection, const char *query) {
  MYSQL_RES *mysql_result;
  MYSQL_ROW row;
  char *result = NULL;

  mysql_result = xb_mysql_query(connection, query, true);

  ut_ad(mysql_num_fields(mysql_result) == 1);

  if ((row = mysql_fetch_row(mysql_result))) {
    result = strdup(row[0]);
  }

  mysql_free_result(mysql_result);

  return (result);
}
