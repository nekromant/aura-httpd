#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>

struct json_object *json_find(json_object *arr, char *k)
{
	json_object_object_foreach(arr, key, val) {
		if (strcmp(key, k) == 0)
			return val;
	}
	return NULL;
}

const char *json_find_string(json_object *o, char *k)
{
	json_object *tmp = json_find(o, k);
	if (!tmp)
		return NULL;
	return json_object_get_string(tmp);
}

void ahttpd_allow_method(struct evhttp_request *request, enum evhttp_cmd_type tp)
{
	if (tp != evhttp_request_get_command(request))
		evhttp_send_error(request, 405, "Method not allowed. Check ur docs");
}

void ahttpd_add_path(struct ahttpd_mountpoint *mpoint, const char *path,
			void (*cb)(struct evhttp_request *request, void *privParams), void *arg)
{
	char *str;
	int ret = asprintf(&str, "%s%s", mpoint->mountpoint, path);
	if (ret == -1)
		BUG(NULL, "asprintf() failed!");
	slog(4, SLOG_DEBUG, "Adding path %s ", str);
	evhttp_set_cb (mpoint->server->eserver, str, cb, arg);
	free(str);
}

void ahttpd_del_path(struct ahttpd_mountpoint *mpoint, const char *path)
{
	char *str;
	int ret = asprintf(&str, "%s/%s", mpoint->mountpoint, path);
	if (ret == -1)
		BUG(NULL, "asprintf() failed!");

	evhttp_del_cb (mpoint->server->eserver, str);
	free(str);
}

void ahttpd_reply_accepted(struct evhttp_request *request, const char *redir)
{
	struct evbuffer *buffer = evbuffer_new();
	evhttp_add_header (evhttp_request_get_output_headers (request),
			"Location", redir);
	evhttp_send_reply(request, 202, "Accepted", buffer);
	evbuffer_free (buffer);
}


void ahttpd_reply_with_json(struct evhttp_request *request, json_object *o)
{
	struct evbuffer *buffer = evbuffer_new();
	const char *str = json_object_to_json_string(o);
	evbuffer_add(buffer, str, strlen(str));
	evhttp_add_header (evhttp_request_get_output_headers (request),
			"Content-Type", "application/json");
	evhttp_send_reply(request, HTTP_OK, "OK", buffer);
	evbuffer_free (buffer);
}
