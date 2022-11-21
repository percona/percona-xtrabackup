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

#include "xbcloud/s3_ec2.h"
#include "msg.h"
#include "my_rapidjson_size_t.h"
#include "rapidjson/document.h"

namespace xbcloud {
bool S3_ec2_instance::parse_var_response(const Http_response &http_response,
                                         std::string &var) {
  if (http_response.body().size() == 0) {
    return false;
  }

  if (http_response.headers().count("content-type") == 0 ||
      http_response.headers().at("content-type") != "text/plain") {
    return false;
  }
  var = std::string(http_response.body().begin(), http_response.body().end());

  return true;
}

bool S3_ec2_instance::parse_keys_response(const Http_response &http_response) {
  if (http_response.body().size() == 0) {
    return false;
  }

  if (http_response.headers().count("content-type") == 0 ||
      http_response.headers().at("content-type") != "text/plain") {
    msg_ts("%s: Failed to parse content-type for instance metadata\n",
           my_progname);
    return false;
  }
  std::string s(http_response.body().begin(), http_response.body().end());
  rapidjson::Document document;
  document.Parse(s.c_str());
  if (!document.IsObject()) {
    msg_ts("%s: Instance metadata response is not a JSON object\n",
           my_progname);
    return false;
  }

  if (!document.HasMember("Code")) {
    msg_ts("%s: Instance metadata response missing Code element\n",
           my_progname);
    return false;
  }
  if (!document.HasMember("AccessKeyId")) {
    msg_ts("%s: Instance metadata response missing AccessKeyId element\n",
           my_progname);
    return false;
  }
  if (!document.HasMember("SecretAccessKey")) {
    msg_ts("%s: Instance metadata response missing SecretAccessKey element\n",
           my_progname);
    return false;
  }
  if (!document.HasMember("Token")) {
    msg_ts("%s: Instance metadata response missing TokTokenen element\n",
           my_progname);
    return false;
  }

  std::string code = document["Code"].GetString();

  if (code.compare("Success") != 0) {
    msg_ts("%s: Instance metadata field Code did not return Success\n",
           my_progname);
    return false;
  }

  access_key = document["AccessKeyId"].GetString();
  secret_key = document["SecretAccessKey"].GetString();
  session_token = document["Token"].GetString();

  return true;
}

bool S3_ec2_instance::fetch_metadata() {
  /* token */
  Http_request token_req(Http_request::PUT, Http_request::HTTP, host,
                         token_url);
  token_req.add_header("X-aws-ec2-metadata-token-ttl-seconds",
                       std::to_string(token_ttl));
  Http_response token_resp;
  if (!http_client->make_request(token_req, token_resp)) {
    return false;
  }

  if (!token_resp.ok()) return false;

  if (!parse_var_response(token_resp, token)) return false;

  /* profile id */
  Http_request profile_req(Http_request::GET, Http_request::HTTP, host,
                           metadata_url);
  profile_req.add_header("X-aws-ec2-metadata-token", token);
  Http_response profile_resp;
  if (!http_client->make_request(profile_req, profile_resp)) {
    return false;
  }

  if (!profile_resp.ok()) return false;

  if (!parse_var_response(profile_resp, profile)) return false;

  /* metadata */
  Http_request metadata_req(Http_request::GET, Http_request::HTTP, host,
                            metadata_url + profile);
  metadata_req.add_header("X-aws-ec2-metadata-token", token);
  Http_response metadata_resp;
  if (!http_client->make_request(metadata_req, metadata_resp)) {
    msg_ts("%s: Failed to fetch instance metadata\n", my_progname);
    return false;
  }

  if (!metadata_resp.ok()) return false;

  if (!parse_keys_response(metadata_resp)) return false;

  is_ec2_instance_with_profile = true;

  return true;
}
}  // namespace xbcloud
