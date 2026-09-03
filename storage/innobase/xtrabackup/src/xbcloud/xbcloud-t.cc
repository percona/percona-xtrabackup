#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "azure.h"
#include "http.h"
#include "s3.h"
#include "swift.h"

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

MATCHER(IsPost, "") { return arg.method() == Http_request::POST; }

MATCHER(IsHead, "") { return arg.method() == Http_request::HEAD; }

MATCHER_P(PayloadEq, payload, "") {
  return (std::string(&arg.payload()[0], arg.payload().size()) == payload);
}

TEST(s3_client, basicDNSv4) {
  Mock_http_client http_client;
  S3_client c(&http_client, "us-east-1", "my-access-key-id", "my-secret-key", 1,
              1);
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
  c.probe_api_version_and_lookup("probe-bucket");

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
  S3_client c(&http_client, "us-east-1", "my-access-key-id", "my-secret-key", 1,
              1);
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
  c.probe_api_version_and_lookup("probe-bucket");

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
  S3_client c(&http_client, "us-east-1", "my-access-key-id", "my-secret-key", 1,
              1);
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
  c.probe_api_version_and_lookup("probe-bucket");

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

TEST(s3v4_signer, sessionToken) {
  Http_request req(Http_request::GET, Http_request::HTTPS, "hyhost",
                   "mybucket/myobject/");
  req.add_header("Content-Length", "4");
  req.add_header("Content-Type", "application/octet-stream");
  req.append_payload("test", 4);

  S3_signerV4 signer(LOOKUP_PATH, "example-region", "access_key", "secret_key",
                     "session_token");

  signer.sign_request("myhost", "mybucket", req, 1555892546);

  ASSERT_STREQ(req.headers().at("Authorization").c_str(),
               "AWS4-HMAC-SHA256 "
               "Credential=access_key/20190422/example-region/s3/aws4_request, "
               "SignedHeaders=content-length;content-type;host;x-amz-content-"
               "sha256;x-amz-date;x-amz-security-token, "
               "Signature="
               "891034a3bd13729689a54d363380ad1849b26bf2b9e461d4c2bdeeca32e0c1c"
               "e");
}

TEST(s3v4_signer, storageClass) {
  Http_request req(Http_request::GET, Http_request::HTTPS, "hyhost",
                   "mybucket/myobject/");
  req.add_header("Content-Length", "4");
  req.add_header("Content-Type", "application/octet-stream");
  req.append_payload("test", 4);

  S3_signerV4 signer(LOOKUP_PATH, "example-region", "access_key", "secret_key",
                     "session_token", "storage_class");

  signer.sign_request("myhost", "mybucket", req, 1555892546);

  ASSERT_STREQ(
      req.headers().at("Authorization").c_str(),
      "AWS4-HMAC-SHA256 "
      "Credential=access_key/20190422/example-region/s3/aws4_request, "
      "SignedHeaders=content-length;content-type;host;x-amz-content-"
      "sha256;x-amz-date;x-amz-security-token;x-amz-storage-class, "
      "Signature="
      "fe6c888f22fb23a7a3fe6f663013b5df3cc761c3777ed4368b325114c885320f");
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

const char *keystone_v3_resp =
    "{"
    "   \"token\":{"
    "      \"is_domain\":false,"
    "      \"methods\":["
    "         \"password\""
    "      ],"
    "      \"roles\":["
    "         {"
    "            \"id\":\"xxxxxxxxxxxxxxxx\","
    "            \"name\":\"user\""
    "         }"
    "      ],"
    "      \"expires_at\":\"2019-05-01T22:47:55.000000Z\","
    "      \"project\":{"
    "         \"domain\":{"
    "            \"id\":\"default\","
    "            \"name\":\"Default\""
    "         },"
    "         \"id\":\"cccccccccccccccccccccccccccc\","
    "         \"name\":\"jenkins-test\""
    "      },"
    "      \"catalog\":["
    "         {"
    "            \"endpoints\":["
    "               {"
    "                  \"region_id\":\"RegionOne\","
    "                  \"url\":\"https://example.com:9696\","
    "                  \"region\":\"RegionOne\","
    "                  \"interface\":\"public\","
    "                  \"id\":\"3658503dc5f54bb7a1f5c3877469ec7d\""
    "               },"
    "               {"
    "                  \"region_id\":\"RegionOne\","
    "                  \"url\":\"https://example.com:9696\","
    "                  \"region\":\"RegionOne\","
    "                  \"interface\":\"admin\","
    "                  \"id\":\"9ab4964231ea4f259a8bbd0220f5ea6d\""
    "               },"
    "               {"
    "                  \"region_id\":\"RegionOne\","
    "                  \"url\":\"https://example.com:9696\","
    "                  \"region\":\"RegionOne\","
    "                  \"interface\":\"internal\","
    "                  \"id\":\"cc192a46b63146cfa07b81ccabd4ce28\""
    "               }"
    "            ],"
    "            \"type\":\"network\","
    "            \"id\":\"07f1221740784c5c9f8f04e2d16b48d3\","
    "            \"name\":\"neutron\""
    "         },"
    "         {"
    "            \"endpoints\":["
    "               {"
    "                  \"region_id\":\"RegionOne\","
    "                  "
    "\"url\":\"https://example.com:8080/v1/"
    "AUTH_67a0195262ec4452bd2af48e75bcb687\","
    "                  \"region\":\"RegionOne\","
    "                  \"interface\":\"internal\","
    "                  \"id\":\"476e37810fb7485e812856bb4a7e477e\""
    "               },"
    "               {"
    "                  \"region_id\":\"RegionOne\","
    "                  "
    "\"url\":\"https://example.com:8080/v1/"
    "AUTH_67a0195262ec4452bd2af48e75bcb687\","
    "                  \"region\":\"RegionOne\","
    "                  \"interface\":\"public\","
    "                  \"id\":\"7122b0327b8a4ab396c29b547359d0db\""
    "               },"
    "               {"
    "                  \"region_id\":\"RegionOne\","
    "                  "
    "\"url\":\"https://example.com:8080/v1/"
    "AUTH_67a0195262ec4452bd2af48e75bcb687\","
    "                  \"region\":\"RegionOne\","
    "                  \"interface\":\"admin\","
    "                  \"id\":\"d92fe3eaa1bc4be297c34ae7f042b27b\""
    "               }"
    "            ],"
    "            \"type\":\"object-store\","
    "            \"id\":\"78a8ab157b9447ceb8d771f8422cbd98\","
    "            \"name\":\"swift\""
    "         },"
    "         {"
    "            \"endpoints\":["
    "               {"
    "                  \"region_id\":\"RegionOne\","
    "                  \"url\":\"https://example.com:5000/v3/\","
    "                  \"region\":\"RegionOne\","
    "                  \"interface\":\"internal\","
    "                  \"id\":\"2a9c09ad4be04d66b3ed09e561d756b1\""
    "               },"
    "               {"
    "                  \"region_id\":\"RegionOne\","
    "                  \"url\":\"https://example.com:5000/v3/\","
    "                  \"region\":\"RegionOne\","
    "                  \"interface\":\"public\","
    "                  \"id\":\"ae2e556c56c34e48a8de4d5da9addc63\""
    "               },"
    "               {"
    "                  \"region_id\":\"RegionOne\","
    "                  \"url\":\"https://example.com:35357/v3/\","
    "                  \"region\":\"RegionOne\","
    "                  \"interface\":\"admin\","
    "                  \"id\":\"b52a04b9059741dcb989ba1dae513b24\""
    "               }"
    "            ],"
    "            \"type\":\"identity\","
    "            \"id\":\"zzzzzzzzzzzzzzzzzzzzzz\","
    "            \"name\":\"keystone\""
    "         }"
    "      ],"
    "      \"user\":{"
    "         \"password_expires_at\":null,"
    "         \"domain\":{"
    "            \"id\":\"default\","
    "            \"name\":\"Default\""
    "         },"
    "         \"id\":\"xxxxxxxxxxxxxxxxxxxxxxx\","
    "         \"name\":\"batman\""
    "      },"
    "      \"audit_ids\":["
    "         \"xxxxxxxxxxxxxxxxx\""
    "      ],"
    "      \"issued_at\":\"2019-05-01T21:47:55.000000Z\""
    "   }"
    "}";

TEST(keystone_v3, unscoped_keystone_v3_success) {
  Mock_http_client http_client;
  Keystone_client k(&http_client, "https://example.com/");
  k.set_user("batman");
  k.set_password("swordfish");
  k.set_domain("user-domain");
  Keystone_client::auth_info_t auth_info;

  EXPECT_CALL(
      http_client,
      make_request(
          AllOf(IsPost(),
                Property(&Http_request::url,
                         StrEq("https://example.com/auth/tokens/")),
                PayloadEq("{\"auth\":{\"identity\":{\"methods\":[\"password\"],"
                          "\"password\":{\"user\":{\"name\":\"batman\","
                          "\"domain\":{\"name\":\"user-domain\"},\"password\":"
                          "\"swordfish\"}}}}}")),
          Response(
              201, keystone_v3_resp,
              std::vector<const char *>{"x-subject-token: test-auth-token"})))
      .WillOnce(Return(true));
  k.auth_v3("", auth_info);
  EXPECT_EQ(auth_info.token, "test-auth-token");
  EXPECT_EQ(
      auth_info.url,
      "https://example.com:8080/v1/AUTH_67a0195262ec4452bd2af48e75bcb687");
}

TEST(keystone_v3, project_scoped_keystone_v3_success) {
  Mock_http_client http_client;
  Keystone_client k(&http_client, "https://example.com/");
  k.set_user("batman");
  k.set_password("swordfish");
  k.set_domain("user-domain");
  k.set_project("example-project");
  Keystone_client::auth_info_t auth_info;

  EXPECT_CALL(
      http_client,
      make_request(
          AllOf(IsPost(),
                Property(&Http_request::url,
                         StrEq("https://example.com/auth/tokens/")),
                PayloadEq("{\"auth\":{\"identity\":{\"methods\":[\"password\"],"
                          "\"password\":{\"user\":{\"name\":\"batman\","
                          "\"domain\":{\"name\":\"user-domain\"},\"password\":"
                          "\"swordfish\"}}},\"scope\":{\"project\":{\"name\":"
                          "\"example-project\"}}}}")),
          Response(
              201, keystone_v3_resp,
              std::vector<const char *>{"x-subject-token: test-auth-token"})))
      .WillOnce(Return(true));
  k.auth_v3("", auth_info);
  EXPECT_EQ(auth_info.token, "test-auth-token");
  EXPECT_EQ(
      auth_info.url,
      "https://example.com:8080/v1/AUTH_67a0195262ec4452bd2af48e75bcb687");
}

TEST(keystone_v3, project_scoped_with_project_domain_keystone_v3_success) {
  Mock_http_client http_client;
  Keystone_client k(&http_client, "https://example.com/");
  k.set_user("batman");
  k.set_password("swordfish");
  k.set_domain("user-domain");
  k.set_project("example-project");
  k.set_project_domain("project-domain-example");
  Keystone_client::auth_info_t auth_info;

  EXPECT_CALL(
      http_client,
      make_request(
          AllOf(IsPost(),
                Property(&Http_request::url,
                         StrEq("https://example.com/auth/tokens/")),
                PayloadEq("{\"auth\":{\"identity\":{\"methods\":[\"password\"],"
                          "\"password\":{\"user\":{\"name\":\"batman\","
                          "\"domain\":{\"name\":\"user-domain\"},\"password\":"
                          "\"swordfish\"}}},\"scope\":{\"project\":{\"name\":"
                          "\"example-project\",\"domain\":{\"name\":\"project-"
                          "domain-example\"}}}}}")),
          Response(
              201, keystone_v3_resp,
              std::vector<const char *>{"x-subject-token: test-auth-token"})))
      .WillOnce(Return(true));
  k.auth_v3("", auth_info);
  EXPECT_EQ(auth_info.token, "test-auth-token");
  EXPECT_EQ(
      auth_info.url,
      "https://example.com:8080/v1/AUTH_67a0195262ec4452bd2af48e75bcb687");
}

TEST(azure_client, basicDNSv4) {
  Mock_http_client http_client;
  Azure_client c(&http_client, "my-storage-account", "my-access-key-id", 0,
                 "my-storage-class", 1, 1);

  EXPECT_CALL(
      http_client,
      make_request(
          AllOf(
              IsPut(),
              Property(&Http_request::url,
                       StrEq("https://my-storage-account.blob.core.windows.net/"
                             "test-b?restype=container"))),
          Response(200, "", std::vector<char *>{})))
      .WillOnce(Return(true));
  c.create_container("test-b");

  Http_buffer buf;
  buf.append("some contents");

  EXPECT_CALL(
      http_client,
      make_request(
          AllOf(
              IsPut(),
              Property(
                  &Http_request::url,
                  StrEq(
                      "https://my-storage-account.blob.core.windows.netbucket/"
                      "test-o"))),
          Response(200, "", std::vector<char *>{})))
      .WillOnce(Return(true));
  c.upload_object("bucket", "test-o", buf);
}

TEST(azure_client, basicEndpoint) {
  Mock_http_client http_client;
  Azure_client c(&http_client, "my-storage-account", "my-access-key-id", 0,
                 "my-storage-class", 1, 1);
  c.set_endpoint("https://my-storage-account.endpoint.com", 0,
                 "my-storage-account");

  EXPECT_CALL(
      http_client,
      make_request(
          AllOf(IsPut(),
                Property(&Http_request::url,
                         StrEq("https://my-storage-account.endpoint.com/"
                               "test-b?restype=container"))),
          Response(200, "", std::vector<char *>{})))
      .WillOnce(Return(true));
  c.create_container("test-b");
}

TEST(azure_signer, basicDNS) {
  Http_request req(Http_request::GET, Http_request::HTTPS, "my_host",
                   "my_container/my_object");

  req.add_header("Content-Length", "4");
  req.add_header("Content-Type", "application/octet-stream");
  req.add_header("x-ms-blob-type", "BlockBlob");
  req.append_payload("test", 4);
  Azure_signer signer(
      "my-storage-account",
      "zUfvsKXcAO2RMJCwvnElnG/"
      "Kk7wxQ8V4TPXIuZ53qFwJNtpLUEjYdBe9iGTkMgwUGFHVfFgn2qkgoqDP/i3ODQ==",
      0);
  signer.sign_request("mycontainer", "myblob", req, 1555892546);

  ASSERT_STREQ(
      req.headers().at("Authorization").c_str(),
      "SharedKey "
      "my-storage-account:3RNUoT7aggagRnzDebBh1JDGhJocV0e2MKb9ChNvAcM=");
}

TEST(azure_signer, storageClass) {
  Http_request req(Http_request::GET, Http_request::HTTPS, "my_host",
                   "my_container/my_object");

  req.add_header("Content-Length", "4");
  req.add_header("Content-Type", "application/octet-stream");
  req.add_header("x-ms-blob-type", "BlockBlob");
  req.append_payload("test", 4);
  Azure_signer signer(
      "my-storage-account",
      "zUfvsKXcAO2RMJCwvnElnG/"
      "Kk7wxQ8V4TPXIuZ53qFwJNtpLUEjYdBe9iGTkMgwUGFHVfFgn2qkgoqDP/i3ODQ==",
      0, "storage_class");
  signer.sign_request("mycontainer", "myblob", req, 1555892546);

  ASSERT_STREQ(
      req.headers().at("Authorization").c_str(),
      "SharedKey "
      "my-storage-account:b/lbriWK8eQ9uMx97nP0vR894wm+oYsq99Sz0JoH/9k=");
}
