/******************************************************
Copyright (c) 2019 Percona LLC and/or its affiliates.

HTTP client implementation using cURL.

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

#include "xbcloud/http.h"

#include <algorithm>
#include <cstring>
#include <sstream>

#include "xbcloud/util.h"

#include <my_sys.h>
#include "common.h"

namespace xbcloud {

class Global_curl {
 private:
  CURL *curl{nullptr};
  std::mutex mutex;

 public:
  Global_curl() {}

  bool create() {
    curl = curl_easy_init();
    return (curl != nullptr);
  }

  void cleanup() {
    curl_easy_cleanup(curl);
    curl = nullptr;
  }

  void lock() { mutex.lock(); }

  CURL *get() const { return curl; }

  void unlock() { mutex.unlock(); }
};

static Global_curl global_curl;

bool http_init() {
  curl_global_init(CURL_GLOBAL_ALL);
  return global_curl.create();
}

void http_cleanup() {
  curl_global_cleanup();
  global_curl.cleanup();
}

std::string uri_escape_string(const std::string &s) {
  std::lock_guard<Global_curl> lock(global_curl);
  char *escaped_string =
      curl_easy_escape(global_curl.get(), s.c_str(), s.length());
  std::string result(escaped_string);
  curl_free(escaped_string);
  return result;
}

std::string uri_escape_path(const std::string &path) {
  size_t start = 0;
  size_t end = path.find('/');
  std::string result;

  while (end != std::string::npos) {
    auto part = path.substr(start, end - start);
    result.append(uri_escape_string(part) + '/');
    start = end + 1;
    end = path.find('/', start);
  }
  result.append(uri_escape_string(path.substr(start, end)));

  return result;
}

void Http_request::add_param(const std::string &name,
                             const std::string &value) {
  std::stringstream param;
  param << uri_escape_string(name) << "=" + uri_escape_string(value);
  params_.push_back(param.str());
}

std::string Http_request::query_string() const {
  std::stringstream query_string;
  int idx = 0;
  /* we need to sort query string params for AWS canonical request */
  std::vector<std::string> sorted_params = params_;
  std::sort(sorted_params.begin(), sorted_params.end());
  for (auto &param : sorted_params) {
    if (idx++ > 0) {
      query_string << "&";
    }
    query_string << param;
  }
  return query_string.str();
}

size_t Http_response::header_appender(char *ptr, size_t size, size_t nmemb,
                                      void *data) {
  size_t buflen = size * nmemb;
  size_t colon_pos = buflen;
  for (size_t i = 0; i < buflen; i++) {
    if (ptr[i] == ':') {
      colon_pos = i;
      break;
    }
  }

  if (colon_pos == buflen) {
    return buflen;
  }

  std::string name(ptr, colon_pos);
  std::string val(ptr + colon_pos + 1, buflen - colon_pos - 1);

  trim(name);
  trim(val);

  to_lower(name);

  if (name.empty()) {
    return buflen;
  }

  Http_response *response = reinterpret_cast<Http_response *>(data);
  response->headers_[name] = val;

  if (name == "content-length") {
    long size = atol(val.c_str());
    if (size > 0 && response->body_.capacity() < (size_t)size) {
      response->body_.reserve(size);
    }
  }

  return buflen;
}

void Event_handler::mcode_or_die(CURLMcode code) {
  if (code == CURLM_OK || code == CURLM_CALL_MULTI_PERFORM) {
    return;
  }
  if (code != CURLM_BAD_SOCKET) {
    assert(0);
  }
}

void Event_handler::remove_socket(Curl_socket_info *socket_info) {
  if (socket_info != nullptr) {
    if (socket_info->evset) {
      ev_io_stop(loop, &socket_info->ev);
    }
    delete socket_info;
  }
}

void Event_handler::set_socket(Curl_socket_info *socket_info,
                               curl_socket_t sockfd, CURL *curl_easy,
                               int action) {
  int kind = (action & (int)CURL_POLL_IN ? (int)EV_READ : 0) |
             (action & (int)CURL_POLL_OUT ? (int)EV_WRITE : 0);

  socket_info->sockfd = sockfd;
  socket_info->action = action;
  socket_info->curl_easy = curl_easy;
  if (socket_info->evset) {
    ev_io_stop(loop, &socket_info->ev);
  }
  ev_io_init(&socket_info->ev, Event_handler::ev_socket_callback,
             socket_info->sockfd, kind);
  socket_info->ev.data = this;
  socket_info->evset = true;
  ev_io_start(loop, &socket_info->ev);
}

void Event_handler::add_socket(curl_socket_t sockfd, CURL *curl_easy,
                               int action) {
  Curl_socket_info *socket_info = new Curl_socket_info();
  set_socket(socket_info, sockfd, curl_easy, action);
  curl_multi_assign(curl_multi, sockfd, socket_info);
}

int Event_handler::multi_timer_callback(CURLM *multi, long timeout_ms,
                                        void *data) {
  Event_handler *h = reinterpret_cast<Event_handler *>(data);

  TRACE("%s %ld\n", __PRETTY_FUNCTION__, timeout_ms);

  ev_timer_stop(h->loop, &h->timer_event);
  if (timeout_ms >= 0) {
    double t = timeout_ms / 1000.;
    ev_timer_init(&h->timer_event, ev_timer_callback, t, 0.);
    ev_timer_start(h->loop, &h->timer_event);
  }
  return 0;
}

int Event_handler::multi_socket_callback(CURL *curl_easy, curl_socket_t sockfd,
                                         int what, void *data,
                                         void *socket_data) {
  Event_handler *h = reinterpret_cast<Event_handler *>(data);
  Curl_socket_info *socket_info =
      reinterpret_cast<Curl_socket_info *>(socket_data);

  TRACE("%s curl_easy %p sockfd %d what %d data %p socket_data %p\n",
        __PRETTY_FUNCTION__, curl_easy, sockfd, what, data, socket_data);

  const char *whatstr[] = {"none", "IN", "OUT", "INOUT", "REMOVE"};

  TRACE("socket callback: sockfd=%d curl_easy=%p what=%s ", sockfd, curl_easy,
        whatstr[what]);

  if (what == CURL_POLL_REMOVE) {
    TRACE("\n");
    h->remove_socket(socket_info);
  } else {
    if (socket_info == nullptr) {
      TRACE("Adding data: %s\n", whatstr[what]);
      h->add_socket(sockfd, curl_easy, what);
    } else {
      TRACE("Changing action from %s to %s\n", whatstr[socket_info->action],
            whatstr[what]);
      h->set_socket(socket_info, sockfd, curl_easy, what);
    }
  }
  return 0;
}

void Event_handler::check_multi_info() {
  int msgs_left{0};
  CURLMsg *msg;

  TRACE("REMAINING: %d\n", running_handles);
  while ((msg = curl_multi_info_read(curl_multi, &msgs_left)) != nullptr) {
    Http_connection *conn;
    if (msg->msg == CURLMSG_DONE) {
      char *url = nullptr;
      auto rc = curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &conn);
      assert(rc == CURLE_OK);
      rc = curl_easy_getinfo(msg->easy_handle, CURLINFO_EFFECTIVE_URL, &url);
      if (rc == CURLE_OK) {
        TRACE("DONE: %s => (%d) %s\n", url, msg->data.result, conn->error());
      }
      conn->finalize(msg->data.result);
      curl_multi_remove_handle(curl_multi, msg->easy_handle);
      delete conn;
      n_queued--;
    }
  }
  if (n_queued < max_requests) {
    process_queue();
  }
}

void Event_handler::ev_socket_callback(EV_P_ struct ev_io *io, int events) {
  int action = (events & (int)EV_READ ? (int)CURL_CSELECT_IN : 0) |
               (events & (int)EV_WRITE ? (int)CURL_CSELECT_OUT : 0);

  TRACE("%s io %p events %d\n", __PRETTY_FUNCTION__, io, events);

  Event_handler *h = reinterpret_cast<Event_handler *>(io->data);

  CURLMcode rc;
  do {
    rc = curl_multi_socket_action(h->curl_multi, io->fd, action,
                                  &h->running_handles);
    mcode_or_die(rc);
  } while (rc == CURLM_CALL_MULTI_PERFORM);

  h->check_multi_info();

  if (h->running_handles <= 0 && h->n_queued == 0) {
    TRACE("last transfer done, kill timeout\n");
    ev_timer_stop(h->loop, &h->timer_event);
  }
}

void Event_handler::ev_timer_callback(EV_P_ struct ev_timer *timer,
                                      int events) {
  Event_handler *h = reinterpret_cast<Event_handler *>(timer->data);

  TRACE("%s timer %p events %d\n", __PRETTY_FUNCTION__, timer, events);

  CURLMcode rc;
  do {
    rc = curl_multi_socket_action(h->curl_multi, CURL_SOCKET_TIMEOUT, 0,
                                  &h->running_handles);
    mcode_or_die(rc);
  } while (rc == CURLM_CALL_MULTI_PERFORM);

  h->check_multi_info();
}

void Event_handler::ev_kickoff_callback(EV_P_ struct ev_timer *timer,
                                        int events) {
  Event_handler *h = reinterpret_cast<Event_handler *>(timer->data);

  TRACE("%s kickoff %p events %d\n", __PRETTY_FUNCTION__, timer, events);

  h->loop_running = true;
}

void Event_handler::ev_queue_callback(EV_P_ ev_async *ev, int revents) {
  Event_handler *h = reinterpret_cast<Event_handler *>(ev->data);
  TRACE("%s async queue callback events %d\n", __PRETTY_FUNCTION__, revents);
  h->process_queue();
}

void Event_handler::process_queue() {
  std::lock_guard<std::mutex> guard(queue_mutex);

  while (!queue.empty() && n_queued < max_requests) {
    auto conn = queue.front();
    TRACE("Adding easy %p to multi %p\n", conn->curl_easy(), conn);
    n_queued++;
    CURLMcode rc = curl_multi_add_handle(curl_multi, conn->curl_easy());
    mcode_or_die(rc);
    queue.pop();
  }

  if (final && queue.empty()) {
    ev_async_stop(loop, &queue_event);
  }
}

bool Event_handler::init() {
  loop = ev_loop_new(0);
  if (loop == nullptr) return false;

  curl_multi = curl_multi_init();
  if (curl_multi == nullptr) return false;

  ev_timer_init(&timer_event, Event_handler::ev_timer_callback, 0., 0.);
  timer_event.data = this;
  ev_timer_start(loop, &timer_event);

  ev_timer_init(&kickoff_event, Event_handler::ev_kickoff_callback, 0.1, 0.);
  kickoff_event.data = this;
  ev_timer_start(loop, &kickoff_event);

  ev_async_init(&queue_event, ev_queue_callback);
  queue_event.data = this;
  ev_async_start(loop, &queue_event);

  curl_multi_setopt(curl_multi, CURLMOPT_SOCKETFUNCTION,
                    Event_handler::multi_socket_callback);
  curl_multi_setopt(curl_multi, CURLMOPT_SOCKETDATA, this);

  curl_multi_setopt(curl_multi, CURLMOPT_TIMERFUNCTION,
                    Event_handler::multi_timer_callback);
  curl_multi_setopt(curl_multi, CURLMOPT_TIMERDATA, this);

  return true;
}

Event_handler::~Event_handler() {
  if (loop != nullptr) ev_loop_destroy(loop);
  if (curl_multi != nullptr) curl_multi_cleanup(curl_multi);
}

void Event_handler::main_loop() { ev_loop(loop, 0); }

std::thread Event_handler::run() {
  auto t = std::thread([this]() { main_loop(); });
  while (!loop_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return t;
}

void Event_handler::add_connection(Http_connection *conn, bool nowait) {
  while (true) {
    queue_mutex.lock();
    if (nowait || queue.size() < max_requests + 4) {
      queue.push(conn);
      queue_mutex.unlock();
      ev_async_send(loop, &queue_event);
      break;
    } else {
      queue_mutex.unlock();
      ev_async_send(loop, &queue_event);
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
}

void Event_handler::stop() {
  queue_mutex.lock();
  final = true;
  while (true) {
    if (queue.empty()) {
      queue_mutex.unlock();
      ev_async_send(loop, &queue_event);
      break;
    } else {
      queue_mutex.unlock();
      ev_async_send(loop, &queue_event);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

bool retriable_curl_error(CURLcode rc) {
  if (rc == CURLE_GOT_NOTHING || rc == CURLE_OPERATION_TIMEDOUT ||
      rc == CURLE_RECV_ERROR || rc == CURLE_SEND_ERROR ||
      rc == CURLE_SEND_FAIL_REWIND) {
    return true;
  }
  return false;
}

bool retriable_http_error(long code) {
  if (code == 503 || code == 500 || code == 504 || code == 408) {
    return true;
  }
  return false;
}

void Http_client::async_result_callback(async_callback_t user_callback,
                                        const char *action, Event_handler *h,
                                        CURLcode rc, Http_connection *conn) {
  if (rc != CURLE_OK) {
    msg_ts("%s: Failed to %s. Error: %s\n", my_progname, action,
           curl_easy_strerror(rc));
  }
  if (user_callback) {
    user_callback(rc, conn);
  }
}

void Http_client::setup_request(
    CURL *curl, const Http_request &request, Http_response &response,
    curl_slist *&headers, Http_connection::upload_state_t *upload_state) const {
  for (auto &header : request.headers()) {
    headers = curl_slist_append(headers,
                                (header.first + ": " + header.second).c_str());
  }

  curl_easy_setopt(curl, CURLOPT_URL, request.url().c_str());

  switch (request.method()) {
    case Http_request::GET:
      break;
    case Http_request::PUT:
      curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
      upload_state->data = &request.payload()[0];
      upload_state->len = request.payload().size();
      curl_easy_setopt(curl, CURLOPT_READFUNCTION,
                       Http_client::upload_callback);
      curl_easy_setopt(curl, CURLOPT_READDATA, upload_state);
      curl_easy_setopt(curl, CURLOPT_INFILESIZE, upload_state->len);
      break;
    case Http_request::POST:
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      break;
    case Http_request::DELETE:
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
      break;
    case Http_request::HEAD:
      curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
      break;
  }

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  if (request.method() == Http_request::POST) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, &request.payload()[0]);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                     (long)request.payload().size());
  }

  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
                   Http_response::header_appender);

  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Http_response::body_appender);

  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

#if LIBCURL_VERSION_NUM <= 0x071506
  curl_easy_setopt(curl, CURLOPT_ENCODING, "gzip");
#else
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "gzip");
#endif

  if (verbose) {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  }

  if (insecure) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }

  if (!cacert.empty()) {
    curl_easy_setopt(curl, CURLOPT_CAINFO, cacert.c_str());
  }
}

int Http_client::upload_callback(char *ptr, size_t size, size_t nmemb,
                                 void *data) {
  Http_connection::upload_state_t *upload =
      reinterpret_cast<Http_connection::upload_state_t *>(data);
  size_t len = std::min(size * nmemb, upload->len);

  TRACE("%s ptr %p size %zu nmemb %zu data %p\n", __PRETTY_FUNCTION__, ptr,
        size, nmemb, data);

  memcpy(ptr, upload->data, len);
  upload->data += len;
  upload->len -= len;

  return len;
}

bool Http_client::make_request(const Http_request &request,
                               Http_response &response) const {
  curl_slist *headers = nullptr;
  Http_connection::upload_state_t upload_state;

  if (!curl) {
    curl_easy_unique_ptr tmp = make_curl_easy();
    curl = std::move(tmp);
  }
  if (!curl) {
    msg("error: cannot initialize curl handler\n");
    return false;
  }

  setup_request(curl.get(), request, response, headers, &upload_state);

  auto res = curl_easy_perform(curl.get());
  if (res != CURLE_OK) {
    msg("error: http request failed: %s\n", curl_easy_strerror(res));
    curl_slist_free_all(headers);
    return false;
  }

  long http_code;
  curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);

  curl_slist_free_all(headers);
  curl_easy_reset(curl.get());

  response.set_http_code(http_code);
  return true;
}

bool Http_client::make_async_request(const Http_request &request,
                                     Http_response &response, Event_handler *h,
                                     async_callback_t callback,
                                     bool nowait) const {
  curl_slist *headers = nullptr;

  auto curl = make_curl_easy();
  if (!curl) {
    msg("error: cannot initialize curl handler\n");
    return false;
  }

  using std::placeholders::_1;
  using std::placeholders::_2;

  auto cb =
      std::bind(async_result_callback, callback, "upload object", h, _1, _2);

  auto conn = new Http_connection(std::move(curl), request, response, cb);
  setup_request(conn->curl_easy(), request, response, headers,
                conn->upload_state());

  conn->set_headers(headers);
  h->add_connection(conn, nowait);

  return true;
}

}  // namespace xbcloud
