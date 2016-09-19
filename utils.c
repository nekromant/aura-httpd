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

long json_find_number(json_object *o, char *k)
{
	json_object *tmp = json_find(o, k);
	if (!tmp)
		return -1;
	return json_object_get_int64(o);
}

bool json_find_boolean(json_object *o, char *k)
{
	json_object *tmp = json_find(o, k);
	if (!tmp)
		return -1;
	return json_object_get_boolean(o);
}


int ahttpd_method_allowed(struct evhttp_request *request, enum evhttp_cmd_type tp)
{
	int ret = (tp & evhttp_request_get_command(request));
	if (!ret)
		evhttp_send_error(request, 405, "Method not allowed. Check ur docs");
	return ret;
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
	int ret = asprintf(&str, "%s%s", mpoint->mountpoint, path);
	if (ret == -1)
		BUG(NULL, "asprintf() failed!");

	slog(4, SLOG_DEBUG, "Removing path %s ", str);
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

struct json_object *json_load_from_file(const char *filename)
{
	json_object *conf = NULL;
	slog(1, SLOG_DEBUG, "Reading config: %s", filename);

	FILE *fd = fopen(filename, "r");
	if (!fd) {
		slog(0, SLOG_ERROR, "Failed to open config file %s: %s",
			filename, strerror(errno));
		goto err_bailout;
	}

	fseek(fd, 0, SEEK_END);
	unsigned long file_size = ftell(fd);
	rewind(fd);

	char *buf = calloc(1, file_size + 1);
	if (!buf)
		goto err_close_file;

	if (1 != fread(buf, file_size, 1, fd)) {
		slog(0, SLOG_ERROR, "Failed to read everything from config file");
		goto err_free_buf;
	}

	enum json_tokener_error error = json_tokener_success;
	conf = json_tokener_parse_verbose(buf, &error);
	if (error != json_tokener_success) {
		slog(0, SLOG_ERROR, "Problem parsing config file");
	}

err_free_buf:
	free(buf);
err_close_file:
	fclose(fd);
err_bailout:
	return conf;
}
