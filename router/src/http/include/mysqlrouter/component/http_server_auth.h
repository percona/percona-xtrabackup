/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_HTTP_INCLUDE_MYSQLROUTER_COMPONENT_HTTP_SERVER_AUTH_H_
#define ROUTER_SRC_HTTP_INCLUDE_MYSQLROUTER_COMPONENT_HTTP_SERVER_AUTH_H_

#include <memory>

#include "http/base/request.h"

#include "mysqlrouter/http_server_lib_export.h"

class HttpAuthRealm;

/**
 * high-level Authentication frontend.
 *
 * bridges HttpRequest with the HttpAuthRealm's
 */
class HTTP_SERVER_LIB_EXPORT HttpAuth {
 public:
  /**
   * require Authorization.
   *
   * @returns if request has been handled
   * @retval true request handled
   */
  static bool require_auth(http::base::Request &req,
                           std::shared_ptr<HttpAuthRealm> realm);
};

#endif  // ROUTER_SRC_HTTP_INCLUDE_MYSQLROUTER_COMPONENT_HTTP_SERVER_AUTH_H_
