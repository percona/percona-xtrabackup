/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

<<<<<<< HEAD:unittest/gunit/keyring/mock_logger.h
#ifndef MOCK_LOGGER_H
#define MOCK_LOGGER_H
||||||| 824e2b40640:unittest/gunit/keyring/mock_logger.h
#ifndef MOCKLOGGER_H
#define MOCKLOGGER_H
=======
#ifndef AUTHENTICATION_WEBAUTHN_CLIENTOPT_CASE_H
#define AUTHENTICATION_WEBAUTHN_CLIENTOPT_CASE_H
>>>>>>> mysql-8.4.0:client/include/authentication_webauthn_clientopt-case.h

case OPT_AUTHENTICATION_WEBAUTHN_CLIENT_PRESERVE_PRIVACY:
  opt_authentication_webauthn_client_preserve_privacy =
      (argument != disabled_my_option);
  break;

<<<<<<< HEAD:unittest/gunit/keyring/mock_logger.h
#include <sql/derror.h>
#include "plugin/keyring/common/logger.h"

namespace keyring {
class Mock_logger : public ILogger {
 public:
  MOCK_METHOD2(log, void(longlong level, const char *msg));

  void log(longlong level, longlong errcode, ...) override {
    char buf[LOG_BUFF_MAX];
    const char *fmt = error_message_for_error_log(errcode);

    va_list vl;
    va_start(vl, errcode);
    vsnprintf(buf, LOG_BUFF_MAX - 1, fmt, vl);
    va_end(vl);

    log(level, buf);
  }
};
}  // namespace keyring
#endif  // MOCK_LOGGER_H
||||||| 824e2b40640:unittest/gunit/keyring/mock_logger.h
#include <sql/derror.h>
#include "plugin/keyring/common/logger.h"

namespace keyring {
class Mock_logger : public ILogger {
 public:
  MOCK_METHOD2(log, void(longlong level, const char *msg));

  void log(longlong level, longlong errcode, ...) override {
    char buf[LOG_BUFF_MAX];
    const char *fmt = error_message_for_error_log(errcode);

    va_list vl;
    va_start(vl, errcode);
    vsnprintf(buf, LOG_BUFF_MAX - 1, fmt, vl);
    va_end(vl);

    log(level, buf);
  }
};
}  // namespace keyring
#endif  // MOCKLOGGER_H
=======
#endif /* AUTHENTICATION_WEBAUTHN_CLIENTOPT_CASE_H */
>>>>>>> mysql-8.4.0:client/include/authentication_webauthn_clientopt-case.h
