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

#ifndef XTRABACKUP_BACKUP_CONN_H
#define XTRABACKUP_BACKUP_CONN_H

#include <mysql.h>

#include <list>
#include <optional>
#include <string>

namespace xb {

/** The server a connection is made to. */
enum class Destination {
  MAIN,   /** the server being backed up */
  HISTORY /** the server holding PERCONA_SCHEMA.xtrabackup_history */
};

/** What a connection is for. It is named at the end of the line a connection
announces itself with, so that the several connections a backup opens can be
told apart in the log. */
enum class Purpose {
  BACKUP,        /** the main connection the backup runs on */
  MDL_LOCK,      /** holds the MDL of --lock-ddl-per-table */
  QUERY_KILLER,  /** kills queries older than --kill-long-queries-timeout */
  REDO_ARCHIVE,  /** drives innodb_redo_log_archive_* */
  REDO_CONSUMER, /** registers the redo log consumer */
  HISTORY_RECORD /** reads and writes PERCONA_SCHEMA.xtrabackup_history */
};

/** The TLS settings of a destination.

Only the trust and identity settings are described per destination. The
protocol level ones, the cipher, the TLS version, the CRL, the FIPS mode, the
SNI name and the session data, always follow --ssl-*, because PXB takes those
options from the server tree and a second copy of the code applying them would
fall behind it. */
struct Tls_overrides {
  bool present{false}; /** whether any of the settings below was named */
  uint mode{SSL_MODE_PREFERRED};
  const char *ca{nullptr};
  const char *capath{nullptr};
  const char *cert{nullptr};
  const char *key{nullptr};
};

/** Where a server is and how to authenticate to it. The strings point at the
option globals, which live as long as the process does, so a Dsn owns nothing
and is free to copy. */
struct Dsn {
  const char *host{nullptr};
  const char *user{nullptr};
  const char *password{nullptr};
  uint port{0};
  const char *socket{nullptr};
  Tls_overrides tls{};

  /** Apply the TLS settings to a handle that has not connected yet. The
  --ssl-* options are the baseline for every destination, applied by the
  server's own code so that an option added there does not have to be repeated
  here; a destination that names TLS settings of its own replaces the trust and
  identity ones on top of that.
  @param[in,out]	mysql	handle to configure
  @return 0 on success */
  int apply_tls(MYSQL *mysql) const;
};

/** A connection to a MySQL server, closed when it goes out of scope.

It converts to MYSQL * so that it can be handed to xb_mysql_query() and the
other functions below that take a handle, which is what keeps the class out of
the way of the many places that already pass a connection around. The
invariant that pays for the conversion is that mysql_close() is called in
exactly one place in the tree, Connection::close(). */
class Connection {
 public:
  Connection() = default;
  ~Connection() { close(); }

  /* A connection is neither copied nor moved. It is created where it lives and
  it stays there. Either operation would close a connection that the other end
  goes on using, and a closed connection silently drops everything the session
  holds: LOCK TABLES FOR BACKUP, LOCK INSTANCE FOR BACKUP and the open
  transaction that --lock-ddl-per-table holds its MDL in. The backup would
  carry on unprotected with nothing logged. */
  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;
  Connection(Connection &&) = delete;
  Connection &operator=(Connection &&) = delete;

  operator MYSQL *() const { return m_mysql; }

  bool is_open() const { return m_mysql != nullptr; }
  Destination destination() const { return m_destination; }
  Purpose purpose() const { return m_purpose; }

  /** Close the connection. Idempotent: the handle is nulled, so the close made
  by the destructor after an explicit one does nothing. */
  void close();

  /** The privileges effective for the account this connection authenticated
  as, read on the first call and remembered. Main thread only.
  @return the SHOW GRANTS lines */
  const std::list<std::string> &granted_privileges();

 private:
  friend bool connect(Purpose purpose, Connection &connection);

  MYSQL *m_mysql{nullptr};
  Destination m_destination{Destination::MAIN};
  Purpose m_purpose{Purpose::BACKUP};
  std::optional<std::list<std::string>> m_granted_privileges{};
};

/** Reject the option combinations that cannot be served. Called before
anything is copied.
@return true if the options are usable */
bool validate_connection_options();

/** Whether the user described this destination. MAIN always; HISTORY only
when at least one of the --history-* options was named.
@param[in]	destination	server to ask about
@return true if it was described */
bool is_configured(Destination destination);

/** Open a connection. The caller owns it and decides how long to keep it.

Which server it goes to follows from what it is for, so a purpose belonging
to the backup cannot be asked of the history server by mistake.
@param[in]	purpose		what the connection is for
@param[in,out]	connection	connection to open, which must not be
open already
@return true if the connection could be made */
bool connect(Purpose purpose, Connection &connection);

/** Open the connections that live for the whole backup.
@return true if every one of them could be opened */
bool open_connections();

/** Close the connections that live for the whole backup. */
void close_connections();

}  // namespace xb

/** The connection the backup runs on. */
xb::Connection &main_conn();

/** The connection the backup history record is read and written over. */
xb::Connection &history_conn();

/** A server variable to read and the place to put its value. */
struct mysql_variable {
  const char *name;
  char **value;
};

MYSQL_RES *xb_mysql_query(MYSQL *connection, const char *query, bool use_result,
                          bool die_on_error = true);

my_ulonglong xb_mysql_numrows(MYSQL *connection, const char *query,
                              bool die_on_error);

char *read_mysql_one_value(MYSQL *connection, const char *query);

/** Read mysql_variable from MYSQL_RES.
@param[in]	mysql_result	result to read from
@param[in,out]	vars		variables to look for
@param[in]	vertical_result	whether the result has name and value columns
@return number of rows consumed */
int read_mysql_variables_from_result(MYSQL_RES *mysql_result,
                                     mysql_variable *vars,
                                     bool vertical_result);

void read_mysql_variables(MYSQL *connection, const char *query,
                          mysql_variable *vars, bool vertical_result);

void free_mysql_variables(mysql_variable *vars);

#endif /* XTRABACKUP_BACKUP_CONN_H */
