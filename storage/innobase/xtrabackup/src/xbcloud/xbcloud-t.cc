#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "http.h"
#include "s3.h"

using namespace xbcloud;
using namespace ::testing;

class Mock_http_client : public xbcloud::Http_client {
 public:
  MOCK_CONST_METHOD2(make_request, bool(const Http_request &, Http_response &));
};

MATCHER_P3(Response, code, body, headers, "") {
  arg.set_http_code(code);
  Http_response::body_appender((char *)body, 1, strlen(body), &arg);
  for (auto header : headers) {
    Http_response::header_appender((char *)header, 1, strlen(header), &arg);
  }
  return true;
}

MATCHER_P(HasHeader, name, "") { return arg.headers().count(name) == 1; }

MATCHER(IsGet, "") { return arg.method() == Http_request::GET; }

MATCHER(IsPut, "") { return arg.method() == Http_request::PUT; }

MATCHER(IsHead, "") { return arg.method() == Http_request::HEAD; }

TEST(s3_client, basicDNSv4) {
  Mock_http_client http_client;
  S3_client c(&http_client, "us-east-1", "my-access-key-id", "my-secret-key");
  EXPECT_CALL(
      http_client,
      make_request(
          AllOf(IsHead(),
                Property(
                    &Http_request::url,
                    StrEq("https://probe-bucket.s3.us-east-1.amazonaws.com/")),
                HasHeader("Authorization")),
          Response(200, "", std::vector<char *>{})))
      .WillOnce(Return(true));
  c.probe_api_version_and_lookup();

  EXPECT_CALL(
      http_client,
      make_request(
          AllOf(IsPut(),
                Property(&Http_request::url,
                         StrEq("https://test-b.s3.us-east-1.amazonaws.com/"))),
          Response(200, "", std::vector<char *>{})))
      .WillOnce(Return(true));
  c.create_bucket("test-b");

  Http_buffer buf;
  buf.append("some contents");

  EXPECT_CALL(
      http_client,
      make_request(
          AllOf(IsPut(),
                Property(&Http_request::url,
                         StrEq("https://b.s3.us-east-1.amazonaws.com/test-o"))),
          Response(200, "", std::vector<char *>{})))
      .WillOnce(Return(true));
  c.upload_object("b", "test-o", buf);
}

TEST(s3_client, basicPATHv4) {
  Mock_http_client http_client;
  S3_client c(&http_client, "us-east-1", "my-access-key-id", "my-secret-key");
  c.set_api_version(S3_V4);
  EXPECT_CALL(
      http_client,
      make_request(
          AllOf(IsHead(),
                Property(
                    &Http_request::url,
                    StrEq("https://probe-bucket.s3.us-east-1.amazonaws.com/")),
                HasHeader("Authorization")),
          Response(400, "", std::vector<char *>{})))
      .WillOnce(Return(true));
  EXPECT_CALL(
      http_client,
      make_request(
          AllOf(IsHead(),
                Property(
                    &Http_request::url,
                    StrEq("https://s3.us-east-1.amazonaws.com/probe-bucket/")),
                HasHeader("Authorization")),
          Response(200, "", std::vector<char *>{})))
      .WillOnce(Return(true));
  c.probe_api_version_and_lookup();

  EXPECT_CALL(
      http_client,
      make_request(
          AllOf(IsPut(),
                Property(&Http_request::url,
                         StrEq("https://s3.us-east-1.amazonaws.com/test-b/"))),
          Response(200, "", std::vector<char *>{})))
      .WillOnce(Return(true));
  c.create_bucket("test-b");

  Http_buffer buf;
  buf.append("some contents");

  EXPECT_CALL(
      http_client,
      make_request(
          AllOf(IsPut(),
                Property(&Http_request::url,
                         StrEq("https://s3.us-east-1.amazonaws.com/b/test-o"))),
          Response(200, "", std::vector<char *>{})))
      .WillOnce(Return(true));
  c.upload_object("b", "test-o", buf);
}

TEST(s3_client, basicEndpoint) {
  Mock_http_client http_client;
  S3_client c(&http_client, "us-east-1", "my-access-key-id", "my-secret-key");
  c.set_endpoint("my.custom.endpoint.com");
  EXPECT_CALL(
      http_client,
      make_request(
          AllOf(IsHead(),
                Property(&Http_request::url,
                         StrEq("https://probe-bucket.my.custom.endpoint.com/")),
                HasHeader("Authorization")),
          Response(200, "", std::vector<char *>{})))
      .WillOnce(Return(true));
  c.probe_api_version_and_lookup();

  EXPECT_CALL(
      http_client,
      make_request(
          AllOf(IsPut(),
                Property(&Http_request::url,
                         StrEq("https://test-b.my.custom.endpoint.com/"))),
          Response(200, "", std::vector<char *>{})))
      .WillOnce(Return(true));
  c.create_bucket("test-b");
}

TEST(s3v4_signer, basicDNS) {
  Http_request req(Http_request::GET, Http_request::HTTPS, "mybucket.hyhost",
                   "myobject/");
  req.add_header("Content-Length", "4");
  req.add_header("Content-Type", "application/octet-stream");
  req.append_payload("test", 4);

  S3_signerV4 signer(LOOKUP_DNS, "example-region", "access_key", "secret_key");

  signer.sign_request("mybucket.myhost", "", req, 1555892546);

  ASSERT_STREQ(req.headers().at("Authorization").c_str(),
               "AWS4-HMAC-SHA256 "
               "Credential=access_key/20190422/example-region/s3/aws4_request, "
               "SignedHeaders=content-length;content-type;host;x-amz-content-"
               "sha256;x-amz-date, "
               "Signature="
               "b1b7e962059c0ea4a02dcf05c81c8890d07c4d488e5f261dc20307c3264c821"
               "1");
}

TEST(s3v4_signer, basicPATH) {
  Http_request req(Http_request::GET, Http_request::HTTPS, "hyhost",
                   "mybucket/myobject/");
  req.add_header("Content-Length", "4");
  req.add_header("Content-Type", "application/octet-stream");
  req.append_payload("test", 4);

  S3_signerV4 signer(LOOKUP_PATH, "example-region", "access_key", "secret_key");

  signer.sign_request("myhost", "mybucket", req, 1555892546);

  ASSERT_STREQ(req.headers().at("Authorization").c_str(),
               "AWS4-HMAC-SHA256 "
               "Credential=access_key/20190422/example-region/s3/aws4_request, "
               "SignedHeaders=content-length;content-type;host;x-amz-content-"
               "sha256;x-amz-date, "
               "Signature="
               "0360d081e45c1407a9b6a43aa5d40389b7a277562c4087f2579f37c7b1a8137"
               "f");
}

TEST(s3v2_signer, basicDNS) {
  Http_request req(Http_request::GET, Http_request::HTTPS, "mybucket.hyhost",
                   "myobject/");
  req.add_header("Content-Length", "4");
  req.add_header("Content-Type", "application/octet-stream");
  req.append_payload("test", 4);

  S3_signerV2 signer(LOOKUP_DNS, "example-region", "access_key", "secret_key");

  signer.sign_request("mybucket.myhost", "", req, 1555892546);

  ASSERT_STREQ(req.headers().at("Authorization").c_str(),
               "AWS access_key:0oalADiTB2mEnSgKkj5mFXEJZU4=");
}

TEST(s3v2_signer, basicPATH) {
  Http_request req(Http_request::GET, Http_request::HTTPS, "hyhost",
                   "mybucket/myobject/");
  req.add_header("Content-Length", "4");
  req.add_header("Content-Type", "application/octet-stream");
  req.append_payload("test", 4);

  S3_signerV2 signer(LOOKUP_PATH, "example-region", "access_key", "secret_key");

  signer.sign_request("myhost", "mybucket", req, 1555892546);

  ASSERT_STREQ(req.headers().at("Authorization").c_str(),
               "AWS access_key:VQ+0g9rlqRH9SMeRubHF2FW9jeI=");
}
