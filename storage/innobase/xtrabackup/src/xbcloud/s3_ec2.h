/******************************************************
Copyright (c) 2022 Percona LLC and/or its affiliates.

AWS EC2 Instance Metadata client implementation using IMDSv2.

More details:
https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/iam-roles-for-amazon-ec2.html


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

#ifndef XBCLOUD_S3_EC2_H
#define XBCLOUD_S3_EC2_H

#include "xbcloud/http.h"

namespace xbcloud {
class S3_ec2_instance {
 private:
  inline static const std::string host = "169.254.169.254";
  inline static const std::string token_url = "/latest/api/token";
  inline static const int token_ttl = 21600;
  inline static const std::string metadata_url =
      "/latest/meta-data/iam/security-credentials/";
  const Http_client *http_client;
  inline static bool is_ec2_instance_with_profile = false;
  std::string token;
  std::string profile;
  std::string access_key;
  std::string secret_key;
  std::string session_token;

  /** parse a HTTP response and replace var with response body
  @param[in]      http_response  Http response from curl call
  @param[in/out]  var            var to be replaced with response body
  @return true in case of suceess, false otherwise. */
  bool parse_var_response(const Http_response &http_response, std::string &var);

  /** parse a HTTP response and update access_key, secret_key & session_token
  @param[in]      http_response  Http response from curl call
  @return true in case of suceess, false otherwise. */
  bool parse_keys_response(const Http_response &http_response);

 public:
  S3_ec2_instance(const Http_client *_http_client)
      : http_client(_http_client) {}

  bool get_is_ec2_instance_with_profile() {
    return is_ec2_instance_with_profile;
  }
  std::string get_access_key() { return access_key; }
  std::string get_secret_key() { return secret_key; }
  std::string get_session_token() { return session_token; }

  /** Query instance metadata, set is_ec2_instance_with_profile in case of
  success and update access_key / secret_key & session_token.
  @return true in case of suceess, false otherwise. */
  bool fetch_metadata();
};
}  // namespace xbcloud
#endif  // XBCLOUD_S3_EC2_H
