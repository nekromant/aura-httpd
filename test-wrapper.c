#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <aura-httpd/server.h>
#include <unistd.h>



static void *script_thread(void *arg)
{
	int script_result = system(arg);
	return (void *) (intptr_t) WEXITSTATUS(script_result);
}

/* This shouldn't give us any memory leaks */
int main(int argc, char *argv[])
{
	pthread_attr_t attrs;
    pthread_attr_init(&attrs);
	pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_JOINABLE);
	pthread_t tid;
	int *ret=NULL;

	slog_init(NULL, 99);

	struct json_object *conf = json_load_from_file(argv[1]);
	struct ahttpd_server *server = ahttpd_server_create(conf);

	pthread_create(&tid, &attrs, script_thread, argv[2]);

	aura_eventloop_dispatch(server->aloop, 0);
	pthread_join(tid, (void **)&ret);
	ahttpd_server_destroy(server);
	json_object_put(conf);

	exit( (int) (intptr_t) ret);
}
