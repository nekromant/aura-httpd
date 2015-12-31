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

static char *host = "127.0.0.1";
static int  port = 8088;



void load_mountpoint(struct event_base *ebase, struct evhttp *eserver, json_object *opts)
{
	const char *mp = json_find_string(opts, "mountpoint");
	const char *tp = json_find_string(opts, "type");

	if ((tp == NULL) || (mp == NULL)) { 
		slog(0, SLOG_WARN, "Misconfigurated node section, skipping");
		return;
	}

	slog(1, SLOG_INFO, "Mounting %s at %s", 
	     tp, 
	     mp);
	
	if (strcmp(tp, "control") == 0) 
		ahttpd_mount_control(ebase, eserver, opts);
	if (strcmp(tp, "node") == 0) 
		ahttpd_mount_node(ebase, eserver, opts);

	//else if (strcmp(tp, "control") == 0)

}

void load_mountpoints(struct event_base *ebase, struct evhttp *eserver, json_object *fstab)
{
	slog(4, SLOG_DEBUG, "Loading mountpoints");
	int i;
	int num = json_object_array_length(fstab);
	for (i=0; i<num; i++) { 
		json_object *j =  json_object_array_get_idx(fstab, i);
		load_mountpoint(ebase, eserver, j);
	}
}

void parse_config(struct event_base *ebase, struct evhttp *eserver, const char *filename)
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
				host = strdup(json_object_get_string(val));
			break;
		case json_type_int:
			if (strcmp(key, "port")==0)
				port = json_object_get_int(val);
			break;
		case json_type_array:
			if (strcmp(key, "mountpoints")==0) {
				slog(0, SLOG_DEBUG, "%s ", key);
				load_mountpoints(ebase, eserver, val);
			}
			break;
		default:
			break;
		}
	}
	slog(1, SLOG_DEBUG, "Serving at %s:%d", host, port);
	json_object_put(conf);
	free(buf);
}

int main (void) {
	struct event_base *ebase;
	struct evhttp *server;

	slog_init(NULL, 99);

	ebase = event_base_new ();
	server = evhttp_new (ebase);
	evhttp_set_allowed_methods (server, EVHTTP_REQ_GET);
	parse_config(ebase, server, "config.json");
	evhttp_set_gencb (server, notfound, 0);

	if (evhttp_bind_socket (server, host, port) != 0) {
		BUG(NULL, "I tried hard, but I could not bind to %s:%d, sorry(");
	}

	event_base_dispatch(ebase);
	evhttp_free (server);
	event_base_free (ebase);
	return 0;
}


