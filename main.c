#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>




struct json_object *load_config(char *filename)
{
	struct stat st;

	slog(1, SLOG_DEBUG, "Reading config: %s", filename);
	if ((stat(filename, &st) == -1) || S_ISDIR(st.st_mode)) {
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

	enum json_tokener_error error = json_tokener_success;
	json_object *conf = json_tokener_parse_verbose(buf, &error);
	if (error != json_tokener_success) {
		slog(0, SLOG_ERROR, "Problem parsing config file");
		exit(1);
	}
	fclose(fd);
	free(buf);
	return conf;
}


int main (void) {
	slog_init(NULL, 99);

	struct json_object *conf = load_config("../config.json");
	struct ahttpd_server *server = ahttpd_server_create(conf);
	json_object_put(conf);
	aura_handle_events_forever(server->aloop);
	ahttpd_server_destroy(server);

	return 0;
}
