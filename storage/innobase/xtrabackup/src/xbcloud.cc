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

#include <my_global.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <ev.h>
#include <unistd.h>
#include <errno.h>
#include <gcrypt.h>
#include <assert.h>
#include <my_sys.h>
#include <my_dir.h>
#include <my_getopt.h>
#include <algorithm>
#include <map>
#include <string>
#include <jsmn.h>
#include "xbstream.h"

using std::min;
using std::max;
using std::map;
using std::string;

#define XBCLOUD_VERSION "1.0"

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
typedef struct container_list_struct container_list;
typedef struct object_info_struct object_info;

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
	bool chunk_uploaded;
	char error[CURL_ERROR_SIZE];
	struct curl_slist *slist;
	char *url;
	char *container;
	char *token;
	char *backup_name;
	char *name;
	gcry_md_hd_t md5;
	char hash[33];
	size_t chunk_no;
	bool magic_verified;
	size_t chunk_path_len;
	xb_chunk_type_t chunk_type;
	size_t payload_size;
	size_t chunk_size;
	bool upload_started;
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

struct object_info_struct {
	char hash[33];
	char name[SWIFT_MAX_URL_SIZE];
	size_t bytes;
};

struct container_list_struct {
	size_t content_length;
	size_t content_bufsize;
	char *content_json;
	size_t object_count;
	object_info *objects;
};

enum {SWIFT, S3};
const char *storage_names[] =
{ "SWIFT", "S3", NullS};

static my_bool opt_verbose = 0;
static ulong opt_storage = SWIFT;
static const char *opt_user = NULL; 
static const char *opt_container = NULL;
static const char *opt_url = NULL;
static const char *opt_key = NULL;
static const char *opt_name = NULL;
static const char *opt_cacert = NULL;
static ulong opt_parallel = 1;
static my_bool opt_insecure = 0;
static enum {MODE_GET, MODE_PUT} opt_mode;

static char **file_list = NULL;
static int file_list_size = 0;

TYPELIB storage_typelib =
{array_elements(storage_names)-1, "", storage_names, NULL};

enum {
	OPT_STORAGE = 256,
	OPT_SWIFT_CONTAINER,
	OPT_SWIFT_URL,
	OPT_SWIFT_KEY,
	OPT_SWIFT_USER,
	OPT_PARALLEL,
	OPT_CACERT,
	OPT_INSECURE,
	OPT_VERBOSE
};


static struct my_option my_long_options[] =
{
	{"help", '?', "Display this help and exit.",
	 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},

	{"storage", OPT_STORAGE, "Specify storage type S3/SWIFT.",
	 &opt_storage, &opt_storage, &storage_typelib,
	 GET_ENUM, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},

	{"swift-container", OPT_SWIFT_CONTAINER,
	 "Swift container.",
	 &opt_container, &opt_container, 0, GET_STR_ALLOC, REQUIRED_ARG,
	 0, 0, 0, 0, 0, 0},

	{"swift-user", OPT_SWIFT_USER,
	 "Swift user.",
	 &opt_user, &opt_user, 0, GET_STR_ALLOC, REQUIRED_ARG,
	 0, 0, 0, 0, 0, 0},

	{"swift-url", OPT_SWIFT_USER,
	 "Swift URL.",
	 &opt_url, &opt_url, 0, GET_STR_ALLOC, REQUIRED_ARG,
	 0, 0, 0, 0, 0, 0},

	{"swift-key", OPT_SWIFT_KEY,
	 "Swift key.",
	 &opt_key, &opt_key, 0, GET_STR_ALLOC, REQUIRED_ARG,
	 0, 0, 0, 0, 0, 0},

	{"parallel", OPT_PARALLEL,
	 "Number of parallel chunk uploads.",
	 &opt_parallel, &opt_parallel, 0, GET_ULONG, REQUIRED_ARG,
	 1, 0, 0, 0, 0, 0},

	{"cacert", OPT_CACERT,
	 "CA certificate file.",
	 &opt_cacert, &opt_cacert, 0, GET_STR_ALLOC, REQUIRED_ARG,
	 0, 0, 0, 0, 0, 0},

	{"insecure", OPT_INSECURE,
	 "Do not verify server SSL certificate.",
	 &opt_insecure, &opt_insecure, 0, GET_BOOL, NO_ARG,
	 0, 0, 0, 0, 0, 0},

	{"verbose", OPT_VERBOSE,
	 "Turn ON cURL tracing.",
	 &opt_verbose, &opt_verbose, 0, GET_BOOL, NO_ARG,
	 0, 0, 0, 0, 0, 0},

	{0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};

static map<string, ulonglong> file_chunk_count;

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
void
print_version()
{
	printf("%s  Ver %s for %s (%s)\n", my_progname, XBCLOUD_VERSION,
	       SYSTEM_TYPE, MACHINE_TYPE);
}

static
void
usage()
{
	print_version();
	puts("Copyright (C) 2015 Percona LLC and/or its affiliates.");
	puts("This software comes with ABSOLUTELY NO WARRANTY. "
	     "This is free software,\nand you are welcome to modify and "
	     "redistribute it under the GPL license.\n");

	puts("Manage backups on Cloud services.\n");

	puts("Usage: ");
	printf("  %s -c put [OPTIONS...] <NAME> upload backup from STDIN into "
	       "the cloud service with given name.\n", my_progname);
	printf("  %s -c get [OPTIONS...] <NAME> [FILES...] stream specified "
	       "backup or individual files from cloud service into STDOUT.\n",
	       my_progname);

	puts("\nOptions:");
	my_print_help(my_long_options);
}

static
my_bool
get_one_option(int optid, const struct my_option *opt __attribute__((unused)),
	       char *argument __attribute__((unused)))
{
	switch (optid) {
	case '?':
		usage();
		exit(0);
	}

	return(FALSE);
}

static
int parse_args(int argc, char **argv)
{
	const char *command;

	if (argc < 2) {
		fprintf(stderr, "Command isn't specified. "
			"Supported commands are put and get\n");
		usage();
		exit(EXIT_FAILURE);
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
		usage();
		exit(EXIT_FAILURE);
	}

	if (strcmp(command, "put") == 0) {
		opt_mode = MODE_PUT;
	} else if (strcmp(command, "get") == 0) {
		opt_mode = MODE_GET;
	} else {
		fprintf(stderr, "Unknown command %s. "
			"Supported commands are put and get\n", command);
		exit(EXIT_FAILURE);
	}

	if (handle_options(&argc, &argv, my_long_options, get_one_option)) {
		exit(EXIT_FAILURE);
	}

	/* make sure name is specified */
	if (argc < 1) {
		fprintf(stderr, "object name is required argument\n");
		exit(EXIT_FAILURE);
	}
	opt_name = argv[0];
	argc--; argv++;

	/* validate arguments */
	if (opt_storage == SWIFT) {
		if (opt_user == NULL) {
			fprintf(stderr, "user for Swift is not specified\n");
			exit(EXIT_FAILURE);
		}
		if (opt_container == NULL) {
			fprintf(stderr,
				"container for Swift is not specified\n");
			exit(EXIT_FAILURE);
		}
		if (opt_url == NULL) {
			fprintf(stderr, "URL for Swift is not specified\n");
			exit(EXIT_FAILURE);
		}
		if (opt_key == NULL) {
			fprintf(stderr, "key for Swift is not specified\n");
			exit(EXIT_FAILURE);
		}
	} else {
		fprintf(stderr, "Swift is only supported storage API\n");
	}

	if (argc > 0) {
		file_list = argv;
		file_list_size = argc;
	}

	return(0);
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
size_t swift_auth_header_read_cb(char *ptr, size_t size, size_t nmemb,
				 void *data)
{
	swift_auth_info *info = (swift_auth_info*)(data);

	get_http_header("X-Storage-Url: ", ptr,
			info->url, array_elements(info->url));
	get_http_header("X-Auth-Token: ", ptr,
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
write_null_cb(char *buffer, size_t size, size_t nmemb, void *stream) {
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
static void conn_buffer_updated(connection_info *conn);

static connection_info *get_current_connection(global_io_info *global)
{
	connection_info *conn = global->current_connection;
	ulong i;

	if (conn && conn->filled_size < conn->chunk_size)
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

	if (conn->filled_size < conn->chunk_size) {
		if (revents & EV_READ) {
			ssize_t nbytes = read(io_global->input_fd,
					      conn->buffer + conn->filled_size,
					      conn->chunk_size -
					      conn->filled_size);
			if (nbytes > 0) {
				gcry_md_write(conn->md5,
					      conn->buffer + conn->filled_size,
					      nbytes);
				conn->filled_size += nbytes;
				conn_buffer_updated(conn);
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

	assert(conn->filled_size <= conn->chunk_size);
}

static int swift_upload_read_cb(char *ptr, size_t size, size_t nmemb,
				void *data)
{
	size_t realsize;

	connection_info *conn = (connection_info*)(data);

	if (conn->filled_size == conn->upload_size &&
	    conn->upload_size < conn->chunk_size && !conn->global->eof) {
		ssize_t nbytes;
		assert(conn->global->current_connection == conn);
		do {
			nbytes = read(conn->global->input_fd,
				      conn->buffer + conn->filled_size,
				      conn->chunk_size - conn->filled_size);
		} while (nbytes == -1 && errno == EAGAIN);
		if (nbytes > 0) {
			gcry_md_write(conn->md5,
				      conn->buffer + conn->filled_size,
				      nbytes);
			conn->filled_size += nbytes;
			conn_buffer_updated(conn);
		} else {
			conn->global->eof = 1;
		}
	}

	realsize = min(size * nmemb, conn->filled_size - conn->upload_size);

	memcpy(ptr, conn->buffer + conn->upload_size, realsize);
	conn->upload_size += realsize;

	if (conn->upload_size == conn->chunk_size ||
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

			conn->chunk_uploaded = true;
		}
	}

	assert(conn->filled_size <= conn->chunk_size);
	assert(conn->upload_size <= conn->filled_size);

	return realsize;
}

static
size_t upload_header_read_cb(char *ptr, size_t size, size_t nmemb,
			     void *data)
{
	connection_info *conn = (connection_info*)(data);
	char etag[33];

	if (get_http_header("Etag: ", ptr, etag, array_elements(etag)) &&
	    strcmp(etag, conn->hash) != 0) {
	    	/* TODO: md5 comparison should be smart to handle delayed
	    	responses from server */
	}

	return nmemb * size;
}

static int conn_upload_init(connection_info *conn)
{
	conn->filled_size = 0;
	conn->upload_size = 0;
	conn->chunk_uploaded = 0;
	conn->chunk_size = CHUNK_HEADER_CONSTANT_LEN;
	conn->magic_verified = false;
	conn->chunk_path_len = 0;
	conn->chunk_type = XB_CHUNK_TYPE_UNKNOWN;
	conn->payload_size = 0;
	conn->upload_started = false;
	conn->chunk_no = conn->global->chunk_no++;

	return 0;
}

static int conn_upload_start(connection_info *conn)
{
	char object_url[SWIFT_MAX_URL_SIZE];
	CURLMcode rc;

	file_chunk_count[conn->name]++;

	snprintf(object_url, array_elements(object_url), "%s/%s/%s/%s.%020llu",
		 conn->url, conn->container, conn->backup_name, conn->name,
		 file_chunk_count[conn->name]);

	conn->easy = curl_easy_init();
	if (!conn->easy) {
		fprintf(stderr, "curl_easy_init() failed\n");
		return 1;
	}
	curl_easy_setopt(conn->easy, CURLOPT_URL, object_url);
	curl_easy_setopt(conn->easy, CURLOPT_READFUNCTION,
							swift_upload_read_cb);
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

	conn->upload_started = true;

	return 0;
}

static void conn_cleanup(connection_info *conn)
{
	if (conn) {
		free(conn->url);
		free(conn->container);
		free(conn->token);
		free(conn->backup_name);
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
		if ((conn->backup_name = strdup(name)) == NULL)
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

/*********************************************************************//**
Handle input buffer updates. Parse chunk header and set appropriate
buffer size. */
static
void
conn_buffer_updated(connection_info *conn)
{
	bool ready_for_upload = false;

	/* chunk header */
	if (!conn->magic_verified &&
	    conn->filled_size >= CHUNK_HEADER_CONSTANT_LEN) {
		if (strncmp(XB_STREAM_CHUNK_MAGIC, conn->buffer,
			sizeof(XB_STREAM_CHUNK_MAGIC) - 1) != 0) {

			fprintf(stderr, "Error: magic expected\n");
			exit(EXIT_FAILURE);
		}
		conn->magic_verified = true;
		conn->chunk_path_len = uint4korr(conn->buffer
						 + PATH_LENGTH_OFFSET);
		conn->chunk_type = (xb_chunk_type_t)
					(conn->buffer[CHUNK_TYPE_OFFSET]);
		conn->chunk_size = CHUNK_HEADER_CONSTANT_LEN +
					conn->chunk_path_len;
		if (conn->chunk_type != XB_CHUNK_TYPE_EOF) {
			conn->chunk_size += 16;
		}
	}

	/* ordinary chunk */
	if (conn->magic_verified &&
	    conn->payload_size == 0 &&
	    conn->chunk_type != XB_CHUNK_TYPE_EOF &&
	    conn->filled_size >= CHUNK_HEADER_CONSTANT_LEN
					+ conn->chunk_path_len + 16) {

		conn->payload_size = uint8korr(conn->buffer +
					CHUNK_HEADER_CONSTANT_LEN + 
					conn->chunk_path_len);

		conn->chunk_size = conn->payload_size + 4 + 16 +
					conn->chunk_path_len +
					CHUNK_HEADER_CONSTANT_LEN;

		conn->name = (char*)(malloc(conn->chunk_path_len + 1));
		memcpy(conn->name, conn->buffer + CHUNK_HEADER_CONSTANT_LEN,
			conn->chunk_path_len);
		conn->name[conn->chunk_path_len] = 0;

		if (conn->buffer_size < conn->chunk_size) {
			conn->buffer =
			      (char *)(realloc(conn->buffer, conn->chunk_size));
			conn->buffer_size = conn->chunk_size;
		}
		ready_for_upload = true;
	}

	/* EOF chunk has no payload */
	if (conn->magic_verified &&
	    conn->chunk_type == XB_CHUNK_TYPE_EOF &&
	    conn->filled_size >= CHUNK_HEADER_CONSTANT_LEN
					+ conn->chunk_path_len) {


		conn->name = (char*)(malloc(conn->chunk_path_len + 1));
		memcpy(conn->name, conn->buffer + CHUNK_HEADER_CONSTANT_LEN,
			conn->chunk_path_len);
		conn->name[conn->chunk_path_len] = 0;
		ready_for_upload = true;
	}

	/* start upload once recieved the size of the chunk */
	if (!conn->upload_started && ready_for_upload) {
		conn_upload_start(conn);
	}
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

static int cmp_chunks(const void *c1, const void *c2)
{
	return ((slo_chunk*)(c1))->idx - ((slo_chunk*)(c2))->idx;
}

static
int swift_upload_parts(swift_auth_info *auth, const char *container,
		       const char *name)
{
	global_io_info io_global;
	ulong i;
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

	slo_manifest_free(io_global.manifest);

	return 0;
}


/*********************************************************************//**
Callback to parse header of GET request on swift contaier. */
static
size_t swift_container_list_header_cb(char *ptr, size_t size, size_t nmemb,
				      void *data)
{
	container_list *list = (container_list*)(data);
	char object_count_str[100];
	char *endptr;

	if (get_http_header("X-Container-Object-Count: ", ptr,
			object_count_str, sizeof(object_count_str))) {
		list->object_count = strtoull(object_count_str, &endptr, 10);
	}

	return nmemb * size;
}

struct download_buffer_info {
	off_t offset;
	size_t size;
	size_t result_len;
	char *buf;
	curl_read_callback custom_header_callback;
	void *custom_header_callback_data;
};

/*********************************************************************//**
Callback to parse header of GET request on swift contaier. */
static
size_t fetch_buffer_header_cb(char *ptr, size_t size, size_t nmemb,
				      void *data)
{
	download_buffer_info *buffer_info = (download_buffer_info*)(data);
	size_t buf_size;
	char content_length_str[100];
	char *endptr;

	if (get_http_header("Content-Length: ", ptr,
			content_length_str, sizeof(content_length_str))) {

		buf_size = strtoull(content_length_str, &endptr, 10);

		if (buffer_info->buf == NULL) {
			buffer_info->buf = (char*)(malloc(buf_size));
			buffer_info->size = buf_size;
		}

		if (buf_size > buffer_info->size) {
			buffer_info->buf = (char*)
				(realloc(buffer_info->buf, buf_size));
			buffer_info->size = buf_size;
		}

		buffer_info->result_len = buf_size;
	}

	if (buffer_info->custom_header_callback) {
		buffer_info->custom_header_callback(ptr, size, nmemb,
				buffer_info->custom_header_callback_data);
	}

	return nmemb * size;
}

/*********************************************************************//**
Write contents into string buffer */
static
size_t
fetch_buffer_cb(char *buffer, size_t size, size_t nmemb, void *out_buffer)
{
	download_buffer_info *buffer_info = (download_buffer_info*)(out_buffer);

	assert(buffer_info->size >= buffer_info->offset + size * nmemb);

	memcpy(buffer_info->buf + buffer_info->offset, buffer, size * nmemb);
	buffer_info->offset += size * nmemb;

	return size * nmemb;
}


/*********************************************************************//**
Downloads contents of URL into buffer. Caller is responsible for
deallocating the buffer.
@return	pointer to a buffer or NULL */
static
char *
swift_fetch_into_buffer(swift_auth_info *auth, const char *url,
			char **buf, size_t *buf_size, size_t *result_len,
			curl_read_callback header_callback,
			void *header_callback_data)
{
	char auth_token[SWIFT_MAX_HDR_SIZE];
	download_buffer_info buffer_info;
	struct curl_slist *slist = NULL;
	long http_code;
	CURL *curl;
	CURLcode res;

	memset(&buffer_info, 0, sizeof(buffer_info));
	buffer_info.buf = *buf;
	buffer_info.size = *buf_size;
	buffer_info.custom_header_callback = header_callback;
	buffer_info.custom_header_callback_data = header_callback_data;

	snprintf(auth_token, array_elements(auth_token), "X-Auth-Token: %s",
		 auth->token);

	curl = curl_easy_init();

	if (curl != NULL) {
		slist = curl_slist_append(slist, auth_token);

		curl_easy_setopt(curl, CURLOPT_VERBOSE, opt_verbose);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fetch_buffer_cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer_info);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
				 fetch_buffer_header_cb);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA,
				 &buffer_info);
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

	if (res == CURLE_OK) {
		*buf = buffer_info.buf;
		*buf_size = buffer_info.size;
		*result_len = buffer_info.result_len;
		return(buffer_info.buf);
	}

	free(buffer_info.buf);
	*buf = NULL;
	*buf_size = 0;
	*result_len = 0;

	return(NULL);
}

static
container_list *
container_list_new()
{
	container_list *list =
			(container_list *)(calloc(1, sizeof(container_list)));

	return(list);
}

static
void
container_list_free(container_list *list)
{
	free(list->content_json);
	free(list->objects);
	free(list);
}


/*********************************************************************//**
Tokenize json string. Return array of tokens. Caller is responsoble for
deallocating the array. */
jsmntok_t *
json_tokenise(char *json, size_t len, int initial_tokens)
{
	jsmn_parser parser;
	jsmn_init(&parser);

	unsigned int n = initial_tokens;
	jsmntok_t *tokens = (jsmntok_t *)(malloc(sizeof(jsmntok_t) * n));

	int ret = jsmn_parse(&parser, json, len, tokens, n);

	while (ret == JSMN_ERROR_NOMEM)
	{
		n = n * 2 + 1;
		tokens = (jsmntok_t*)(realloc(tokens, sizeof(jsmntok_t) * n));
		ret = jsmn_parse(&parser, json, len, tokens, n);
	}

	if (ret == JSMN_ERROR_INVAL) {
		fprintf(stderr, "error: invalid JSON string\n");

	}
	if (ret == JSMN_ERROR_PART) {
		fprintf(stderr, "error: truncated JSON string\n");
	}

	return tokens;
}

/*********************************************************************//**
Return true if token representation equal to given string. */
static
bool
json_token_eq(const char *buf, jsmntok_t *t, const char *s)
{
	size_t len = strlen(s);

	return((size_t)(t->end - t->start) == len &&
		(strncmp(buf + t->start, s, len) == 0));
}

/*********************************************************************//**
Copy given token as string. */
static
bool
json_token_str(const char *buf, jsmntok_t *t, char *out, int out_size)
{
	memcpy(out, buf + t->start, min(t->end - t->start, out_size - 1));
	out[min(t->end - t->start, out_size - 1)] = 0;

	return(true);
}

static
int
cmp_object_names(const void *c1, const void *c2)
{
	return(strcmp(((object_info*)(c1))->name, ((object_info*)(c2))->name));
}

/*********************************************************************//**
Parse SWIFT container list response and fill output array with values
sorted by object name. */
static
bool
swift_parse_container_list(container_list *list)
{
	list->objects =
		(object_info*)(calloc(list->object_count, sizeof(object_info)));

	if (list->objects == NULL) {
		fprintf(stderr, "error: out of memory\n");
		return(false);
	}

	/*
	[
		{
			"hash": string,
			"bytes": number,
			"name": string
		},
		...
	]
	*/

	jsmntok_t *tokens = json_tokenise(list->content_json,
					  list->content_length,
					  list->object_count * 10);

	int object_idx = 0;

	if (tokens[0].type != JSMN_ARRAY) {
		fprintf(stderr, "error: malformed response "
				"(array expected)\n");
		return(false);
	}

	for (int i = 1; object_idx < tokens[0].size; i++, object_idx++) {
		jsmntok_t *t = &tokens[i];

		if (t->type != JSMN_OBJECT) {
			fprintf(stderr, "error: malformed response "
					"(object expected)\n");
			return(false);
		}

		for (int j = i + 1; j <= i + t->size * 2; j += 2) {
			jsmntok_t *key = &tokens[j];
			jsmntok_t *val = &tokens[j + 1];
			if (key->type != JSMN_STRING) {
				fprintf(stderr, "error: malformed response "
						"(string expected)\n");
				return(false);
			}
			if (json_token_eq(list->content_json, key, "hash")) {
				if (val->end - val->start != 32) {
					fprintf(stderr, "error: malformed "
							"response "
							"(hash must be 32 "
							"chars long)\n");
					return(false);
				}
				json_token_str(list->content_json, val,
					       list->objects[object_idx].hash,
					       33);
			}
			if (json_token_eq(list->content_json, key, "name")) {
				json_token_str(list->content_json, val,
					       list->objects[object_idx].name,
					       SWIFT_MAX_URL_SIZE);
			}
			if (json_token_eq(list->content_json, key, "bytes")) {
				char bytes_str[40];
				char *endptr;
				json_token_str(list->content_json, val,
					       bytes_str, sizeof(bytes_str));
				list->objects[object_idx].bytes =
					strtoull(bytes_str, &endptr, 10);
			}
		}
		i += t->size * 2;
	}

	qsort(list->objects, list->object_count,
		sizeof(object_info), cmp_object_names);

	return(true);
}

/*********************************************************************//**
List swift container with given name. Return list of objects sorted by
object name. */
static
container_list *
swift_list(swift_auth_info *auth, const char *container)
{
	container_list *list;
	char url[SWIFT_MAX_URL_SIZE];

	list = container_list_new();

	/* download the list in json format */
	snprintf(url, array_elements(url), "%s/%s?format=json",
		 auth->url, container);
	list->content_json = swift_fetch_into_buffer(auth, url,
			&list->content_json, &list->content_bufsize,
			&list->content_length,
			swift_container_list_header_cb, list);
	if (list->content_json == NULL) {
		container_list_free(list);
		return(NULL);
	}

	/* parse downloaded list */
	if (!swift_parse_container_list(list)) {
		fprintf(stderr, "error: unable to parse container list\n");
		container_list_free(list);
		return(NULL);
	}

	return(list);
}


/*********************************************************************//**
Return true if chunk is a part of backup with given name. */
static
bool
chunk_belongs_to(const char *chunk_name, const char *backup_name)
{
	size_t backup_name_len = strlen(backup_name);

	return((strlen(chunk_name) > backup_name_len)
		&& (chunk_name[backup_name_len] == '/')
		&& strncmp(chunk_name, backup_name, backup_name_len) == 0);
}

/*********************************************************************//**
Return true if chunk is in given list. */
static
bool
chunk_in_list(const char *chunk_name, char **list, int list_size)
{
	size_t chunk_name_len;

	if (list_size == 0) {
		return(true);
	}

	chunk_name_len = strlen(chunk_name);
	if (chunk_name_len < 20) {
		return(false);
	}

	for (int i = 0; i < list_size; i++) {
		size_t item_len = strlen(list[i]);

		if ((strncmp(chunk_name - item_len + chunk_name_len - 21,
			     list[i], item_len) == 0)
		    && (chunk_name[chunk_name_len - 21] == '.')
		    && (chunk_name[chunk_name_len - item_len - 22] == '/')) {
			return(true);
		}
	}

	return(false);
}

static
int swift_download(swift_auth_info *auth, const char *container,
		   const char *name)
{
	container_list *list;
	char *buf = NULL;
	size_t buf_size = 0;
	size_t result_len = 0;

	if ((list = swift_list(auth, container)) == NULL) {
		return(CURLE_FAILED_INIT);
	}

	for (size_t i = 0; i < list->object_count; i++) {
		const char *chunk_name = list->objects[i].name;

		if (chunk_belongs_to(chunk_name, name)
		    && chunk_in_list(chunk_name, file_list, file_list_size)) {
			char url[SWIFT_MAX_URL_SIZE];

			snprintf(url, sizeof(url), "%s/%s/%s",
				 auth->url, container, chunk_name);

			if ((buf = swift_fetch_into_buffer(
					auth, url, &buf, &buf_size, &result_len,
					NULL, NULL)) == NULL) {
				fprintf(stderr, "error: failed to download "
						"chunk %s\n", chunk_name);
				container_list_free(list);
				return(CURLE_FAILED_INIT);
			}

			fwrite(buf, 1, result_len, stdout);
		}
	}

	free(buf);

	container_list_free(list);

	return(CURLE_OK);
}

/*********************************************************************//**
Check if backup with given name exists.
@return	true if backup exists */
static
bool swift_backup_exists(swift_auth_info *auth, const char *container,
			 const char *backup_name)
{
	container_list *list;

	if ((list = swift_list(auth, container)) == NULL) {
		fprintf(stderr, "Error: unable to list container %s\n",
			container);
		exit(EXIT_FAILURE);
	}

	for (size_t i = 0; i < list->object_count; i++) {
		if (chunk_belongs_to(list->objects[i].name, backup_name)) {
			container_list_free(list);
			return(true);
		}
	}

	container_list_free(list);

	return(false);
}

int main(int argc, char **argv)
{
	swift_auth_info info;
	char auth_url[SWIFT_MAX_URL_SIZE];

	MY_INIT(argv[0]);

	if (parse_args(argc, argv)) {
		return(EXIT_FAILURE);
	}

	if (opt_mode == MODE_PUT) {

		curl_global_init(CURL_GLOBAL_ALL);

		snprintf(auth_url, SWIFT_MAX_URL_SIZE, "%sauth/v1.0/", opt_url);

		if (swift_auth(auth_url, opt_user, opt_key, &info) != 0) {
			fprintf(stderr, "failed to authenticate\n");
			return(EXIT_FAILURE);
		}

		if (swift_create_container(&info, opt_container) != 0) {
			fprintf(stderr, "failed to create container %s\n",
				opt_container);
			return(EXIT_FAILURE);
		}

		if (swift_backup_exists(&info, opt_container, opt_name)) {
			fprintf(stderr, "backup named '%s' already exists!\n",
				opt_name);
			return(EXIT_FAILURE);
		}

		if (swift_upload_parts(&info, opt_container, opt_name) != 0) {
			fprintf(stderr, "upload failed\n");
			return(EXIT_FAILURE);
		}

	} else {

		curl_global_init(CURL_GLOBAL_ALL);

		snprintf(auth_url, SWIFT_MAX_URL_SIZE, "%sauth/v1.0/", opt_url);

		if (swift_auth(auth_url, opt_user, opt_key, &info) != 0) {
			fprintf(stderr, "failed to authenticate\n");
			return(EXIT_FAILURE);
		}

		if (swift_download(&info, opt_container, opt_name)
				   != CURLE_OK) {
			fprintf(stderr, "download failed\n");
			return(EXIT_FAILURE);
		}
	}

	curl_global_cleanup();

	return(EXIT_SUCCESS);
}
