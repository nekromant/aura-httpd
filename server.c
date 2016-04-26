#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>

static void load_mountpoints(struct ahttpd_server *server, json_object *fstab)
{
	slog(4, SLOG_DEBUG, "Loading mountpoints");
	int i;
	int num = json_object_array_length(fstab);
	for (i = 0; i < num; i++) {
		json_object *j = json_object_array_get_idx(fstab, i);
		ahttpd_mount(server, j);
	}
}


static void parse_config(struct ahttpd_server *server, struct json_object *conf)
{
	enum json_type type;

	json_object_object_foreach(conf, key, val) {
		type = json_object_get_type(val);
		switch (type) {
		case json_type_string:
			if (strcmp(key, "host") == 0)
				server->host = strdup(json_object_get_string(val));
			else if (strcmp(key, "index") == 0)
				server->index = strdup(json_object_get_string(val));
			break;
		case json_type_int:
			if (strcmp(key, "port") == 0)
				server->port = json_object_get_int(val);
			break;
		case json_type_array:
			if (strcmp(key, "mountpoints") == 0) {
				slog(0, SLOG_DEBUG, "%s ", key);
				load_mountpoints(server, val);
			}
			break;
		default:
			break;
		}
	}
	slog(1, SLOG_DEBUG, "Serving at %s:%d", server->host, server->port);
}


static void generic_route(struct evhttp_request *request, void *params)
{
	const char *uri = evhttp_request_get_uri(request);
	struct ahttpd_server *server = params;
	struct list_head *h = &server->mountpoints;
	struct ahttpd_mountpoint *pos;

	slog(4, SLOG_DEBUG, "Request: %s", uri);
	list_for_each_entry(pos, h, qentry) {
		if (strncmp(uri, pos->mountpoint, strlen(pos->mountpoint)) == 0) {
			if (pos->fs->route) {
				slog(4, SLOG_DEBUG, "Routing URI %s to mpoint %s",
				     uri, pos->mountpoint);
				pos->fs->route(request, pos);
				return;
			}
		}
	}
	slog(1, SLOG_INFO, "ERROR 404");
	evhttp_send_error(request, HTTP_NOTFOUND, "Not Found");
}


struct ahttpd_server *ahttpd_server_create(struct json_object *config)
{
	struct ahttpd_server *server = calloc(1, sizeof(*server));

	if (!server)
		BUG(NULL, "calloc() failed");

	server->port = 32001;
	server->host = "127.0.0.1";
	server->aloop = aura_eventloop_create_empty();
	server->ebase = aura_eventloop_get_ebase(server->aloop);
	server->eserver = evhttp_new(server->ebase);
	server->mimedb = ahttpd_mime_init();

	if (!server->aloop || !server->ebase || !server->eserver || !server->mimedb)
		BUG(NULL, "fatal error allocating ahttd server instance");

	INIT_LIST_HEAD(&server->mountpoints);

	parse_config(server, config);

	evhttp_set_allowed_methods(server->eserver, EVHTTP_REQ_GET | EVHTTP_REQ_POST | EVHTTP_REQ_PUT);
	evhttp_set_gencb(server->eserver, generic_route, server);

	if (server->index)
		evhttp_set_cb(server->eserver, "/", ahttpd_redirect_cb, server->index);

	if (evhttp_bind_socket(server->eserver, server->host, server->port) != 0)
		BUG(NULL, "I tried hard, but I could not bind to %s:%d, sorry(");

	return server;
}

void ahttpd_server_destroy(struct ahttpd_server *server)
{
	struct ahttpd_mountpoint *pos;
	struct ahttpd_mountpoint *tmp;

	list_for_each_entry_safe(pos, tmp, &server->mountpoints, qentry) {
		ahttpd_unmount(pos);
	}
	if (server->host)
		free(server->host);
	if (server->index)
		free(server->index);
		
	evhttp_free(server->eserver);
	aura_eventloop_destroy(server->aloop);
	ahttpd_mime_destroy(server->mimedb);
	free(server);
}
