/******************************************************
Copyright (c) 2014 Percona LLC and/or its affiliates.

The xbstream utility: serialize/deserialize files in the XBSTREAM format.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*******************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <ev.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <gcrypt.h>
#include <assert.h>
#include <algorithm>
#include <my_global.h>
#include <my_sys.h>

using std::min;
using std::max;

#define SWIFT_MAX_URL_SIZE 2048
#define SWIFT_MAX_HDR_SIZE 2048

#define SWIFT_CHUNK_SIZE 10485760

/*****************************************************************************/

typedef struct swift_auth_info_struct swift_auth_info;
typedef struct connection_info_struct connection_info;
typedef struct socket_info_struct socket_info;
typedef struct global_io_info_struct global_io_info;
typedef struct slo_chunk_struct slo_chunk;
typedef struct slo_manifest_struct slo_manifest;

struct swift_auth_info_struct {
	char url[SWIFT_MAX_URL_SIZE];
	char token[SWIFT_MAX_HDR_SIZE];
};

struct global_io_info_struct {
	struct ev_loop *loop;
	struct ev_io input_event;
	struct ev_timer timer_event;
	CURLM *multi;
	int still_running;
	int eof;
	curl_socket_t input_fd;
	connection_info **connections;
	long chunk_no;
	connection_info *current_connection;
	slo_manifest *manifest;
};

struct socket_info_struct {
	curl_socket_t sockfd;
	CURL *easy;
	int action;
	long timeout;
	struct ev_io ev;
	int evset;
	global_io_info *global;
};

struct connection_info_struct {
	CURL *easy;
	global_io_info *global;
	char *buffer;
	size_t buffer_size;
	size_t filled_size;
	size_t upload_size;
	size_t chunk_uploaded;
	char error[CURL_ERROR_SIZE];
	struct curl_slist *slist;
	char *url;
	char *container;
	char *token;
	char *name;
	gcry_md_hd_t md5;
	char hash[33];
	size_t chunk_no;
};

struct slo_chunk_struct {
	char name[SWIFT_MAX_URL_SIZE];
	char md5[33];
	int idx;
	size_t size;
};

struct slo_manifest_struct {
	DYNAMIC_ARRAY chunks;
	size_t uploaded_idx;
};

static int opt_verbose;
static enum {S3, SWIFT} opt_storage = SWIFT;
static const char *opt_user = NULL; 
static const char *opt_container = NULL;
static const char *opt_url = NULL;
static const char *opt_key = NULL;
static const char *opt_name = NULL;
static const char *opt_cacert = NULL;
static int opt_parallel = 1;
static int opt_insecure = 0;
static enum {MODE_GET, MODE_PUT} opt_mode;

static
slo_manifest *slo_manifest_init()
{
	slo_manifest *manifest = (slo_manifest *)(calloc(1,
							 sizeof(slo_manifest)));
	if (manifest != NULL) {
		my_init_dynamic_array(&manifest->chunks, sizeof(slo_chunk), 50,
				      50);
	}
	return manifest;
}

static
int slo_add_chunk(slo_manifest *manifest, const slo_chunk *chunk)
{
	insert_dynamic(&manifest->chunks, chunk);
	return 0;
}

static
void slo_manifest_free(slo_manifest *manifest)
{
	delete_dynamic(&manifest->chunks);
	free(manifest);
}

static
void usage()
{
	fprintf(stderr, "usage:\n"
		"    xbcloud [options] put/get <name>\n");
}

static
int parse_args(int argc, char **argv)
{
	const char *command;
	int c;

	if (argc < 2) {
		fprintf(stderr, "Command isn't specified. "
			"Supported commands are put and get\n");
		return 1;
	}

	command = argv[1];
	argc--; argv++;

	if (strcmp(command, "put") == 0) {
		opt_mode = MODE_PUT;
	} else if (strcmp(command, "get") == 0) {
		opt_mode = MODE_GET;
	} else {
		fprintf(stderr, "Unknown command %s. "
			"Supported commands are put and get\n", command);
		return 1;
	}

	while (1) {
		enum {
			OPT_HELP = 10,
			OPT_STORAGE,
			OPT_SWIFT_CONTAINER,
			OPT_SWIFT_URL,
			OPT_SWIFT_KEY,
			OPT_SWIFT_USER,
			OPT_PARALLEL,
			OPT_CACERT,
			OPT_INSECURE,
			OPT_VERBOSE
		};
		static struct option long_options[] =
			{
				{"verbose", no_argument, &opt_verbose,
				 OPT_VERBOSE},
				{"help", no_argument, 0, OPT_HELP},
				{"storage", required_argument, 0, OPT_STORAGE},
				{"swift-container", required_argument, 0,
				 OPT_SWIFT_CONTAINER},
				{"swift-user", required_argument, 0,
				 OPT_SWIFT_USER},
				{"swift-url", required_argument, 0,
				 OPT_SWIFT_URL},
				{"swift-key", required_argument, 0,
				 OPT_SWIFT_KEY},
				{"parallel", required_argument, 0,
				 OPT_PARALLEL},
				{"cacert", required_argument, 0, OPT_CACERT},
				{"insecure", no_argument, &opt_insecure,
				 OPT_INSECURE},
				{NULL, 0, 0, 0}
			};

		int option_index = 0;

		c = getopt_long(argc, argv, "s", long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
		case 0:
			break;
		case OPT_STORAGE:
			if (strcmp(optarg, "Swift") == 0) {
				opt_storage = SWIFT;
			} else if (strcmp(optarg, "S3") == 0) {
				opt_storage = S3;
			} else {
				fprintf(stderr, "unknown storage %s "
					"(allowed are Swift and S3)\n", optarg);
				return 1;
			}
			break;
		case OPT_SWIFT_CONTAINER:
			opt_container = optarg;
			break;
		case OPT_SWIFT_USER:
			opt_user = optarg;
			break;
		case OPT_SWIFT_URL:
			opt_url = optarg;
			break;
		case OPT_SWIFT_KEY:
			opt_key = optarg;
			break;
		case OPT_PARALLEL:
			opt_parallel = atoi(optarg);
			if (opt_parallel < 1) {
				fprintf(stderr,
					"wrong value for parallel option %s\n",
					optarg);
				return 1;
			}
			break;
		case OPT_HELP:
			usage();
			return 1;
		case OPT_CACERT:
			opt_cacert = optarg;
			break;
		default:
			return 1;
		}
	}

	/* make sure name is specified */
	while (optind < argc)
		opt_name = argv[optind++];
	if (opt_name == NULL)
	{
		fprintf(stderr, "object name is required argument\n");
		return 1;
	}

	/* validate arguments */
	if (opt_storage == SWIFT) {
		if (opt_user == NULL) {
			fprintf(stderr, "user for Swift is not specified\n");
			return 1;
		}
		if (opt_container == NULL) {
			fprintf(stderr,
				"container for Swift is not specified\n");
			return 1;
		}
		if (opt_url == NULL) {
			fprintf(stderr, "URL for Swift is not specified\n");
			return 1;
		}
		if (opt_key == NULL) {
			fprintf(stderr, "key for Swift is not specified\n");
			return 1;
		}
	}

	return 0;
}

static char *hex_md5(const unsigned char *hash, char *out)
{
	enum { hash_len = 16 };
	char *p;
	int i;

	for (i = 0, p = out; i < hash_len; i++, p+=2) {
		sprintf(p, "%02x", hash[i]);
	}

	return out;
}

/* If header starts with prefix it's value will be copied into output buffer */
static
int get_http_header(const char *prefix, const char *buffer,
		    char *out, size_t out_size)
{
	const char *beg, *end;
	size_t len, prefix_len;

	prefix_len = strlen(prefix);

	if (strncmp(buffer, prefix, prefix_len) == 0) {
		beg = buffer + prefix_len;
		end = strchr(beg, '\r');

		len = min<size_t>(end - beg, out_size - 1);

		strncpy(out, beg, len);

		out[len] = 0;

		return 1;
	}

	return 0;
}

static
size_t swift_auth_header_read_cb(void *ptr, size_t size, size_t nmemb,
				 void *data)
{
	swift_auth_info *info = (swift_auth_info*)(data);
	const char *buffer = (const char *)(ptr);

	get_http_header("X-Storage-Url: ", buffer,
			info->url, array_elements(info->url));
	get_http_header("X-Auth-Token: ", buffer,
			info->token, array_elements(info->token));

	return nmemb * size;
}

static
int
swift_auth(const char *auth_url, const char *username, const char *key,
	   swift_auth_info *info)
{
	CURL *curl;
	CURLcode res;
	long http_code;
	char *hdr_buf = NULL;
	struct curl_slist *slist = NULL;

	curl = curl_easy_init();

	if (curl != NULL) {

		hdr_buf = (char *)(calloc(14 + max(strlen(username),
						   strlen(key)), 1));

		if (!hdr_buf) {
			res = CURLE_FAILED_INIT;
			goto cleanup;
		}

		sprintf(hdr_buf, "X-Auth-User: %s", username);
		slist = curl_slist_append(slist, hdr_buf);

		sprintf(hdr_buf, "X-Auth-Key: %s", key);
		slist = curl_slist_append(slist, hdr_buf);

		curl_easy_setopt(curl, CURLOPT_VERBOSE, opt_verbose);
		curl_easy_setopt(curl, CURLOPT_URL, auth_url);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
				 swift_auth_header_read_cb);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, info);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
		if (opt_cacert != NULL)
			curl_easy_setopt(curl, CURLOPT_CAINFO, opt_cacert);
		if (opt_insecure)
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE);

		res = curl_easy_perform(curl);

		if (res != CURLE_OK) {
			fprintf(stderr, "error: authentication failed: "
				"curl_easy_perform(): %s\n",
				curl_easy_strerror(res));
			goto cleanup;
		}
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		if (http_code != 200) {
			fprintf(stderr, "error: authentication failed "
				"with response code: %ld\n", http_code);
			res = CURLE_LOGIN_DENIED;
			goto cleanup;
		}
	} else {
		res = CURLE_FAILED_INIT;
		fprintf(stderr, "error: curl_easy_init() failed\n");
		goto cleanup;
	}

cleanup:
	if (hdr_buf)
		free(hdr_buf);
	if (slist)
		curl_slist_free_all(slist);
	if (curl)
		curl_easy_cleanup(curl);

	return res;
}

static
size_t
write_null_cb(void *buffer, size_t size, size_t nmemb, void *stream) {
	return nmemb * size;
}

static
int
swift_create_container(swift_auth_info *info, const char *name)
{
	char url[SWIFT_MAX_URL_SIZE];
	char auth_token[SWIFT_MAX_HDR_SIZE];
	CURLcode res;
	long http_code;
	CURL *curl;
	struct curl_slist *slist = NULL;

	snprintf(url, array_elements(url), "%s/%s", info->url, name);
	snprintf(auth_token, array_elements(auth_token), "X-Auth-Token: %s",
		 info->token);

	curl = curl_easy_init();

	if (curl != NULL) {
		slist = curl_slist_append(slist, auth_token);

		curl_easy_setopt(curl, CURLOPT_VERBOSE, opt_verbose);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_null_cb);
		curl_easy_setopt(curl, CURLOPT_PUT, 1L);
		if (opt_cacert != NULL)
			curl_easy_setopt(curl, CURLOPT_CAINFO, opt_cacert);
		if (opt_insecure)
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE);

		res = curl_easy_perform(curl);

		if (res != CURLE_OK) {
			fprintf(stderr,
				"error: curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));
			goto cleanup;
		}
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		if (http_code != 201 && /* created */
		    http_code != 202    /* accepted (already exists) */) {
			fprintf(stderr, "error: request failed "
				"with response code: %ld\n", http_code);
			res = CURLE_LOGIN_DENIED;
			goto cleanup;
		}
	} else {
		res = CURLE_FAILED_INIT;
		fprintf(stderr, "error: curl_easy_init() failed\n");
		goto cleanup;
	}

cleanup:
	if (slist)
		curl_slist_free_all(slist);
	if (curl)
		curl_easy_cleanup(curl);

	return res;
}

/* Check for completed transfers, and remove their easy handles */
static void check_multi_info(global_io_info *g)
{
	char *eff_url;
	CURLMsg *msg;
	int msgs_left;
	connection_info *conn;
	CURL *easy;

	while ((msg = curl_multi_info_read(g->multi, &msgs_left))) {
		if (msg->msg == CURLMSG_DONE) {
			easy = msg->easy_handle;
			curl_easy_getinfo(easy, CURLINFO_PRIVATE, &conn);
			curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL,
					  &eff_url);
			curl_multi_remove_handle(g->multi, easy);
			curl_easy_cleanup(easy);
		}
	}
}

/* Die if we get a bad CURLMcode somewhere */ 
static void mcode_or_die(const char *where, CURLMcode code)
{
	if (code != CURLM_OK)
	{
		const char *s;
		switch (code)
		{
		case CURLM_BAD_HANDLE:
			s = "CURLM_BAD_HANDLE";
			break;
		case CURLM_BAD_EASY_HANDLE:
			s = "CURLM_BAD_EASY_HANDLE";
			break;
		case CURLM_OUT_OF_MEMORY:
			s = "CURLM_OUT_OF_MEMORY";
			break;
		case CURLM_INTERNAL_ERROR:
			s = "CURLM_INTERNAL_ERROR";
			break;
		case CURLM_UNKNOWN_OPTION:
			s = "CURLM_UNKNOWN_OPTION";
			break;
		case CURLM_LAST:
			s = "CURLM_LAST";
			break;
		default:
			s = "CURLM_unknown";
			break;
		case CURLM_BAD_SOCKET:
			s = "CURLM_BAD_SOCKET";
			fprintf(stderr, "error: %s returns %s\n", where, s);
			/* ignore this error */
			return;
		}
		fprintf(stderr, "error: %s returns %s\n", where, s);
		assert(0);
	}
}

/* Called by libev when we get action on a multi socket */ 
static void event_cb(EV_P_ struct ev_io *w, int revents)
{
	global_io_info *global = (global_io_info*)(w->data);
	CURLMcode rc;

#if ((LIBCURL_VERSION_MAJOR >= 7) && (LIBCURL_VERSION_MINOR >= 16))
	int action = (revents & EV_READ  ? CURL_POLL_IN  : 0) |
		(revents & EV_WRITE ? CURL_POLL_OUT : 0);

	rc = curl_multi_socket_action(global->multi, w->fd, action,
				      &global->still_running);
#else
	do {
		rc = curl_multi_socket(global->multi, w->fd,
				       &global->still_running);
	} while (rc == CURLM_CALL_MULTI_PERFORM);
#endif
	mcode_or_die("event_cb: curl_multi_socket_action", rc);
	check_multi_info(global);
	if (global->still_running <= 0) {
		ev_timer_stop(global->loop, &global->timer_event);
	}
}

static void remsock(socket_info *fdp, global_io_info *global)
{
	if (fdp) {
		if (fdp->evset)
			ev_io_stop(global->loop, &fdp->ev);
		free(fdp);
	}
}

static void setsock(socket_info *fdp, curl_socket_t s, CURL *easy, int action,
		    global_io_info *global)
{
	int kind = (action & CURL_POLL_IN  ? (int)(EV_READ)  : 0) |
		(action & CURL_POLL_OUT ? (int)(EV_WRITE) : 0);

	fdp->sockfd = s;
	fdp->action = action;
	fdp->easy = easy;
	if (fdp->evset)
		ev_io_stop(global->loop, &fdp->ev);
	ev_io_init(&fdp->ev, event_cb, fdp->sockfd, kind);
	fdp->ev.data = global;
	fdp->evset = 1;
	ev_io_start(global->loop, &fdp->ev);
}

static void addsock(curl_socket_t s, CURL *easy, int action,
		    global_io_info *global)
{
	socket_info *fdp = (socket_info *)(calloc(sizeof(socket_info), 1));

	fdp->global = global;
	setsock(fdp, s, easy, action, global);
	curl_multi_assign(global->multi, s, fdp);
}

static int sock_cb(CURL *easy, curl_socket_t s, int what, void *cbp,
		   void *sockp)
{
	global_io_info *global = (global_io_info*)(cbp);
	socket_info *fdp = (socket_info*)(sockp);

	if (what == CURL_POLL_REMOVE) {
		remsock(fdp, global);
	} else {
		if (!fdp) {
			addsock(s, easy, what, global);
		} else {
			setsock(fdp, s, easy, what, global);
		}
	}
	return 0;
}

/* Called by libev when our timeout expires */
static void timer_cb(EV_P_ struct ev_timer *w, int revents)
{
	global_io_info *io_global = (global_io_info*)(w->data);
	CURLMcode rc;

#if ((LIBCURL_VERSION_MAJOR >= 7) && (LIBCURL_VERSION_MINOR >= 16))
	rc = curl_multi_socket_action(io_global->multi, CURL_SOCKET_TIMEOUT, 0,
				      &io_global->still_running);
#else
	do {
		rc = curl_multi_socket_all(io_global->multi,
					   &io_global->still_running);
	} while (rc == CURLM_CALL_MULTI_PERFORM);
#endif
	mcode_or_die("timer_cb: curl_multi_socket_action", rc);
	check_multi_info(io_global);
}

static int conn_upload_init(connection_info *conn);

static connection_info *get_current_connection(global_io_info *global)
{
	connection_info *conn = global->current_connection;
	int i;

	if (conn && conn->filled_size < conn->buffer_size)
		return conn;

	for (i = 0; i < opt_parallel; i++) {
		conn = global->connections[i];
		if (conn->filled_size == 0 || conn->chunk_uploaded) {
			global->current_connection = conn;
			conn_upload_init(conn);
			return conn;
		}
	}

	return NULL;
}

/* This gets called whenever data is received from the input */ 
static void input_cb(EV_P_ struct ev_io *w, int revents)
{
	global_io_info *io_global = (global_io_info *)(w->data);
	connection_info *conn = get_current_connection(io_global);

	if (conn == NULL)
		return;

	if (conn->filled_size < conn->buffer_size) {
		if (revents & EV_READ) {
			ssize_t nbytes = read(io_global->input_fd,
					      conn->buffer + conn->filled_size,
					      conn->buffer_size -
					      conn->filled_size);
			if (nbytes > 0) {
				gcry_md_write(conn->md5,
					      conn->buffer + conn->filled_size,
					      nbytes);
				conn->filled_size += nbytes;
			} else if (nbytes < 0) {
				if (errno != EAGAIN && errno != EINTR) {
					/* failed to read input */
					exit(1);
				}
			} else {
				io_global->eof = 1;
				ev_io_stop(io_global->loop, w);
			}
		}
	}

	assert(conn->filled_size <= conn->buffer_size);
}

static int swift_upload_read_cb(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t realsize;

	connection_info *conn = (connection_info*)(data);

	if (conn->filled_size == conn->upload_size &&
	    conn->upload_size < conn->buffer_size && !conn->global->eof) {
		ssize_t nbytes;
		assert(conn->global->current_connection == conn);
		do {
			nbytes = read(conn->global->input_fd,
				      conn->buffer + conn->filled_size,
				      conn->buffer_size - conn->filled_size);
		} while (nbytes == -1 && errno == EAGAIN);
		if (nbytes > 0) {
			gcry_md_write(conn->md5,
				      conn->buffer + conn->filled_size,
				      nbytes);
			conn->filled_size += nbytes;
		} else {
			conn->global->eof = 1;
		}
	}

	realsize = min(size * nmemb, conn->filled_size - conn->upload_size);

	memcpy(ptr, conn->buffer + conn->upload_size, realsize);
	conn->upload_size += realsize;

	if (conn->upload_size == conn->buffer_size ||
	    (conn->global->eof && conn->upload_size == conn->filled_size)) {
		if (!conn->chunk_uploaded && realsize == 0) {
			slo_chunk chunk;

			hex_md5(gcry_md_read(conn->md5, GCRY_MD_MD5),
				conn->hash);
			gcry_md_reset(conn->md5);
			chunk.idx = conn->chunk_no;
			strcpy(chunk.md5, conn->hash);
			sprintf(chunk.name, "%s/%s-%020zu", conn->container,
				conn->name,
				conn->chunk_no);
			chunk.size = conn->upload_size;
			slo_add_chunk(conn->global->manifest, &chunk);

			conn->chunk_uploaded = 1;
		}
	}

	assert(conn->filled_size <= conn->buffer_size);
	assert(conn->upload_size <= conn->filled_size);

	return realsize;
}

static
size_t upload_header_read_cb(void *ptr, size_t size, size_t nmemb,
			     void *data)
{
	connection_info *conn = (connection_info*)(data);
	const char *buffer = (const char *)(ptr);
	char etag[33];

	if (get_http_header("Etag: ", buffer, etag, array_elements(etag)) &&
	    strcmp(etag, conn->hash) != 0) {
		fprintf(stderr, "md5 of uploaded chunk doesn't match\n");
		fprintf(stderr, "%s vs %s\n", etag, conn->hash);
	}

	return nmemb * size;
}

static int conn_upload_init(connection_info *conn)
{
	char object_url[SWIFT_MAX_URL_SIZE];
	CURLMcode rc;

	snprintf(object_url, array_elements(object_url), "%s/%s/%s-%020ld",
		 conn->url, conn->container, conn->name, conn->global->chunk_no);

	conn->filled_size = 0;
	conn->upload_size = 0;
	conn->chunk_uploaded = 0;
	conn->chunk_no = conn->global->chunk_no++;

	conn->easy = curl_easy_init();
	if (!conn->easy) {
		fprintf(stderr, "curl_easy_init() failed\n");
		return 1;
	}
	curl_easy_setopt(conn->easy, CURLOPT_URL, object_url);
	curl_easy_setopt(conn->easy, CURLOPT_READFUNCTION, swift_upload_read_cb);
	curl_easy_setopt(conn->easy, CURLOPT_READDATA, conn);
	curl_easy_setopt(conn->easy, CURLOPT_VERBOSE, opt_verbose);
	curl_easy_setopt(conn->easy, CURLOPT_ERRORBUFFER, conn->error);
	curl_easy_setopt(conn->easy, CURLOPT_PRIVATE, conn);
	curl_easy_setopt(conn->easy, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(conn->easy, CURLOPT_LOW_SPEED_TIME, 3L);
	curl_easy_setopt(conn->easy, CURLOPT_LOW_SPEED_LIMIT, 10L);
	curl_easy_setopt(conn->easy, CURLOPT_PUT, 1L);
	curl_easy_setopt(conn->easy, CURLOPT_HTTPHEADER, conn->slist);
	curl_easy_setopt(conn->easy, CURLOPT_HEADERFUNCTION,
			 upload_header_read_cb);
	curl_easy_setopt(conn->easy, CURLOPT_HEADERDATA, conn);
	if (opt_cacert != NULL)
		curl_easy_setopt(conn->easy, CURLOPT_CAINFO, opt_cacert);
	if (opt_insecure)
		curl_easy_setopt(conn->easy, CURLOPT_SSL_VERIFYPEER, FALSE);

	rc = curl_multi_add_handle(conn->global->multi, conn->easy);
	mcode_or_die("conn_upload_init: curl_multi_add_handle", rc);

#if !((LIBCURL_VERSION_MAJOR >= 7) && (LIBCURL_VERSION_MINOR >= 16))
	do {
		rc = curl_multi_socket_all(conn->global->multi,
					   &conn->global->still_running);
	} while(rc == CURLM_CALL_MULTI_PERFORM);
#endif

	return 0;
}

static void conn_cleanup(connection_info *conn)
{
	if (conn) {
		free(conn->url);
		free(conn->container);
		free(conn->token);
		free(conn->name);
		free(conn->buffer);
		if (conn->easy)
			curl_easy_cleanup(conn->easy);
		if (conn->md5)
			gcry_md_close(conn->md5);
	}
	free(conn);
}

static connection_info *conn_new(const char *url, const char *container,
				 const char *name, const char *token,
				 global_io_info *global)
{
	connection_info *conn;
	char token_header[SWIFT_MAX_HDR_SIZE];

	conn = (connection_info *)(calloc(1, sizeof(connection_info)));
	if (conn != NULL) {
		if ((conn->url = strdup(url)) == NULL)
			goto error;
		if ((conn->container = strdup(container)) == NULL)
			goto error;
		if ((conn->token = strdup(token)) == NULL)
			goto error;
		if ((conn->name = strdup(name)) == NULL)
			goto error;
		if (gcry_md_open(&conn->md5, GCRY_MD_MD5, 0) != 0)
			goto error;
		conn->global = global;
		conn->buffer_size = SWIFT_CHUNK_SIZE;
		if ((conn->buffer = (char *)(calloc(conn->buffer_size, 1))) ==
		    NULL)
			goto error;
		conn->filled_size = 0;
		snprintf(token_header, array_elements(token_header),
			 "X-Auth-Token: %s", token);
		conn->slist = curl_slist_append(conn->slist, token_header);
		conn->slist = curl_slist_append(conn->slist,
						"Connection: keep-alive");
		conn->slist = curl_slist_append(conn->slist,
						"Content-Length: 0");
		conn->slist = curl_slist_append(conn->slist,
						"Content-Type: "
						"application/octet-stream");
	}

	return conn;
error:
	conn_cleanup(conn);
	return NULL;
}

static int init_input(global_io_info *io_global)
{
	ev_io_init(&io_global->input_event, input_cb, STDIN_FILENO, EV_READ);
	io_global->input_event.data = io_global;
	ev_io_start(io_global->loop, &io_global->input_event);

	return 0;
}

/* Update the event timer after curl_multi library calls */ 
static int multi_timer_cb(CURLM *multi, long timeout_ms, global_io_info *global)
{
	ev_timer_stop(global->loop, &global->timer_event);
	if (timeout_ms > 0) {
		double  t = timeout_ms / 1000;
		ev_timer_init(&global->timer_event, timer_cb, t, 0.);
		ev_timer_start(global->loop, &global->timer_event);
	} else {
		timer_cb(global->loop, &global->timer_event, 0);
	}
	return 0;
}

static
int swift_upload_manifest(swift_auth_info *auth, const char *container,
			  const char *name, slo_manifest *manifest);

static int cmp_chunks(const void *c1, const void *c2)
{
	return ((slo_chunk*)(c1))->idx - ((slo_chunk*)(c2))->idx;
}

static
int swift_upload_parts(swift_auth_info *auth, const char *container,
		       const char *name)
{
	global_io_info io_global;
	int i;
#if !((LIBCURL_VERSION_MAJOR >= 7) && (LIBCURL_VERSION_MINOR >= 16))
	long timeout;
	CURLMcode rc;
#endif

	memset(&io_global, 0, sizeof(io_global));

	io_global.manifest = slo_manifest_init();
	io_global.loop = ev_default_loop(0);
	init_input(&io_global);
	io_global.multi = curl_multi_init();
	ev_timer_init(&io_global.timer_event, timer_cb, 0., 0.);
	io_global.timer_event.data = &io_global;
	io_global.connections = (connection_info **)
		(calloc(opt_parallel, sizeof(connection_info)));
	for (i = 0; i < opt_parallel; i++) {
		io_global.connections[i] = conn_new(auth->url, container, name,
						    auth->token, &io_global);
	}

	/* setup the generic multi interface options we want */
	curl_multi_setopt(io_global.multi, CURLMOPT_SOCKETFUNCTION, sock_cb);
	curl_multi_setopt(io_global.multi, CURLMOPT_SOCKETDATA, &io_global);
#if ((LIBCURL_VERSION_MAJOR >= 7) && (LIBCURL_VERSION_MINOR >= 16))
	curl_multi_setopt(io_global.multi, CURLMOPT_TIMERFUNCTION, multi_timer_cb);
	curl_multi_setopt(io_global.multi, CURLMOPT_TIMERDATA, &io_global);
#else
	curl_multi_timeout(io_global.multi, &timeout);
	if (timeout >= 0)
		multi_timer_cb(io_global.multi, timeout, &io_global);
#endif

#if ((LIBCURL_VERSION_MAJOR >= 7) && (LIBCURL_VERSION_MINOR >= 16))
	curl_multi_socket_action(io_global.multi, CURL_SOCKET_TIMEOUT, 0,
				 &io_global.still_running);
#else
	do {
		rc = curl_multi_socket_all(io_global.multi, &io_global.still_running);
	} while(rc == CURLM_CALL_MULTI_PERFORM);
#endif

	ev_loop(io_global.loop, 0);
	curl_multi_cleanup(io_global.multi);

	for (i = 0; i < opt_parallel; i++) {
		connection_info *conn = io_global.connections[i];
		if (conn->upload_size != conn->filled_size) {
			fprintf(stderr, "upload failed: data left in the buffer\n");
			return 1;
		}
	}

	qsort(io_global.manifest->chunks.buffer,
	      io_global.manifest->chunks.elements,
	      sizeof(slo_chunk), cmp_chunks);

	if (swift_upload_manifest(auth, container, name, io_global.manifest) != 0) {
		fprintf(stderr, "failed to create manifest\n");
		return 1;
	}

	slo_manifest_free(io_global.manifest);

	return 0;
}

static
size_t
swift_manifest_read_cb(void *buffer, size_t size, size_t nmemb, void *data)
{
	slo_manifest *manifest = (slo_manifest*)(data);
	const char *prefix;
	const char *suffix;
	slo_chunk *chunk;
	size_t idx;
	int len;

	idx = manifest->uploaded_idx++;

	if (idx >= manifest->chunks.elements)
		return 0;

	prefix = (idx == 0) ? "[" : "";
	suffix = (idx == manifest->chunks.elements - 1) ? "]" : ",";

	chunk = (slo_chunk *)(dynamic_array_ptr(&manifest->chunks, idx));
	len = snprintf(
		(char *)(buffer), nmemb * size,
		"%s{\"path\":\"%s\", \"etag\":\"%s\", \"size_bytes\":%zu}%s",
		prefix,
		chunk->name,
		chunk->md5,
		chunk->size,
		suffix);

	return len;
}

static
int swift_upload_manifest(swift_auth_info *auth, const char *container,
			  const char *name, slo_manifest *manifest)
{
	char url[SWIFT_MAX_URL_SIZE];
	char auth_token[SWIFT_MAX_HDR_SIZE];
	CURLcode res;
	long http_code;
	CURL *curl;
	struct curl_slist *slist = NULL;

	snprintf(url, array_elements(url),
		 "%s/%s/%s?multipart-manifest=put", auth->url, container, name);
	snprintf(auth_token, array_elements(auth_token), "X-Auth-Token: %s",
		 auth->token);

	curl = curl_easy_init();

	if (curl != NULL) {
		slist = curl_slist_append(slist, auth_token);

		curl_easy_setopt(curl, CURLOPT_VERBOSE, opt_verbose);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION,
				 swift_manifest_read_cb);
		curl_easy_setopt(curl, CURLOPT_READDATA, manifest);
		curl_easy_setopt(curl, CURLOPT_PUT, 1L);
		if (opt_cacert != NULL)
			curl_easy_setopt(curl, CURLOPT_CAINFO, opt_cacert);
		if (opt_insecure)
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE);

		res = curl_easy_perform(curl);

		if (res != CURLE_OK) {
			fprintf(stderr, "error: curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));
			goto cleanup;
		}
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		if (http_code != 201 && /* created */
		    http_code != 202    /* accepted (already exists) */) {
			fprintf(stderr, "error: request failed "
				"with response code: %ld\n", http_code);
			res = CURLE_LOGIN_DENIED;
			goto cleanup;
		}
	} else {
		res = CURLE_FAILED_INIT;
		fprintf(stderr, "error: curl_easy_init() failed\n");
		goto cleanup;
	}

cleanup:
	if (slist)
		curl_slist_free_all(slist);
	if (curl)
		curl_easy_cleanup(curl);

	return res;
}

static
size_t
write_download_cb(void *buffer, size_t size, size_t nmemb, void *stream)
{
	FILE *out = (FILE*)(stream);

	return fwrite(buffer, size, nmemb, out);
}


static
int swift_download(swift_auth_info *auth, const char *container,
		   const char *name)
{
	char url[SWIFT_MAX_URL_SIZE];
	char auth_token[SWIFT_MAX_HDR_SIZE];
	CURLcode res;
	long http_code;
	CURL *curl;
	struct curl_slist *slist = NULL;

	snprintf(url, array_elements(url), "%s/%s/%s", auth->url, container,
		 name);
	snprintf(auth_token, array_elements(auth_token), "X-Auth-Token: %s",
		 auth->token);

	curl = curl_easy_init();

	if (curl != NULL) {
		slist = curl_slist_append(slist, auth_token);

		curl_easy_setopt(curl, CURLOPT_VERBOSE, opt_verbose);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_download_cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, stdout);
		if (opt_cacert != NULL)
			curl_easy_setopt(curl, CURLOPT_CAINFO, opt_cacert);
		if (opt_insecure)
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE);

		res = curl_easy_perform(curl);

		if (res != CURLE_OK) {
			fprintf(stderr, "error: curl_easy_perform() failed: %s\n",
				curl_easy_strerror(res));
			goto cleanup;
		}
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		if (http_code != 200) {
			fprintf(stderr, "error: request failed "
				"with response code: %ld\n", http_code);
			res = CURLE_LOGIN_DENIED;
			goto cleanup;
		}
	} else {
		res = CURLE_FAILED_INIT;
		fprintf(stderr, "error: curl_easy_init() failed\n");
		goto cleanup;
	}

cleanup:
	if (slist)
		curl_slist_free_all(slist);
	if (curl)
		curl_easy_cleanup(curl);

	return res;
}

int main(int argc, char **argv)
{
	swift_auth_info info;
	char auth_url[SWIFT_MAX_URL_SIZE];

	if (parse_args(argc, argv))
		return 1;

	if (opt_mode == MODE_PUT) {

		curl_global_init(CURL_GLOBAL_ALL);

		snprintf(auth_url, SWIFT_MAX_URL_SIZE, "%sauth/v1.0/", opt_url);

		if (swift_auth(auth_url, opt_user, opt_key, &info) != 0) {
			fprintf(stderr, "failed to authenticate\n");
			return 1;
		}

		if (swift_create_container(&info, opt_container) != 0) {
			fprintf(stderr, "failed to create container %s\n",
				opt_container);
			return 1;
		}

		if (swift_upload_parts(&info, opt_container, opt_name) != 0) {
			fprintf(stderr, "upload failed\n");
			return 1;
		}

		curl_global_cleanup();

	} else {

		curl_global_init(CURL_GLOBAL_ALL);

		snprintf(auth_url, SWIFT_MAX_URL_SIZE, "%sauth/v1.0/", opt_url);

		if (swift_auth(auth_url, opt_user, opt_key, &info) != 0) {
			fprintf(stderr, "failed to authenticate\n");
			return 1;
		}

		if (swift_download(&info, opt_container, opt_name) != 0) {
			fprintf(stderr, "download failed\n");
			return 1;
		}

		curl_global_cleanup();
	}

	return 0;
}
