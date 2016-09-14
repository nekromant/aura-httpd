#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>

int main (void) {
	slog_init(NULL, 99);

	struct json_object *conf = json_load_from_file("../config.json");
	struct ahttpd_server *server = ahttpd_server_create(conf);
	json_object_put(conf);
	aura_eventloop_dispatch(server->aloop, 0);
	ahttpd_server_destroy(server);

	return 0;
}
