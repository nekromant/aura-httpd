#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>

static void notfound (struct evhttp_request *request, void *params) {
	slog(1, SLOG_INFO, "ERROR 404");
	evhttp_send_error(request, HTTP_NOTFOUND, "Not Found"); 
}

void load_mountpoints(struct ahttpd_server *server, json_object *fstab)
{
	slog(4, SLOG_DEBUG, "Loading mountpoints");
	int i;
	int num = json_object_array_length(fstab);
	for (i=0; i<num; i++) { 
		json_object *j =  json_object_array_get_idx(fstab, i);
		ahttpd_mount(server, j);
	}
}

void parse_config(struct ahttpd_server *server, const char *filename)
{
	struct stat st;
	slog(1, SLOG_DEBUG, "Reading config: %s", filename);
	if ((stat(filename, &st) == -1) || S_ISDIR(st.st_mode) ) {
		slog(0, SLOG_ERROR, "Config file %s doesn't exist");
		exit(1);
	}
	
	int file_size = st.st_size;
	char *buf = calloc(1, file_size + 1);
	FILE *fd = fopen(filename, "r");
	if (!fd) 
		BUG(NULL, "Failed to open config file");
	if (1 != fread(buf, file_size, 1, fd))
		BUG(NULL, "Failed to read config file");
	
	enum json_tokener_error error =  json_tokener_success;
	json_object *conf = json_tokener_parse_verbose(buf, &error);
	if (error != json_tokener_success) { 
		slog(0, SLOG_ERROR, "Problem parsing config file");
		exit(1);
	}
	
	enum json_type type;
	json_object_object_foreach(conf, key, val) {
		type = json_object_get_type(val);
		switch (type) {
		case json_type_string: 
			if (strcmp(key, "host")==0)
				server->host = strdup(json_object_get_string(val));
			break;
		case json_type_int:
			if (strcmp(key, "port")==0)
				server->port = json_object_get_int(val);
			break;
		case json_type_array:
			if (strcmp(key, "mountpoints")==0) {
				slog(0, SLOG_DEBUG, "%s ", key);
				load_mountpoints(server, val);
			}
			break;
		default:
			break;
		}
	}
	slog(1, SLOG_DEBUG, "Serving at %s:%d", server->host, server->port);
	json_object_put(conf);
	free(buf);
}


int main (void) {
	slog_init(NULL, 99);


	struct ahttpd_server *server = calloc(1, sizeof(*server));
	if (!server)
		BUG(NULL, "Calloc() failed");
	
	server->port    = 32001;
	server->host    = "127.0.0.1";
	server->ebase   = event_base_new ();
	server->eserver = evhttp_new (server->ebase);
	INIT_LIST_HEAD(&server->mountpoints);
	evhttp_set_allowed_methods (server->eserver, EVHTTP_REQ_GET);
	parse_config(server, "../config.json");

	evhttp_set_gencb (server->eserver, notfound, 0);

	if (evhttp_bind_socket (server->eserver, server->host, server->port) != 0) {
		BUG(NULL, "I tried hard, but I could not bind to %s:%d, sorry(");
	}

	event_base_dispatch(server->ebase);
	evhttp_free (server->eserver);
	event_base_free (server->ebase);
	return 0;
}


