#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>
#include <unistd.h>


int run_script(char *name, char *arg)
{
    slog(0, SLOG_INFO, "EXEC: %s %s", name, arg);
    pid_t pid;

    if ((pid = fork()) < 0)
        return (-1) ;

    else if (pid > 0)
        return 0;

    setsid() ;   /* become session leader */
    umask(0) ;   /* clear out the file mode creation mask */
    sleep(3); /* Wait until everything goes online */
    execl("/bin/sh", "sh", "-c", name, arg, (char *) 0);
    return(0) ;
}
/* This shouldn't give us any memory leaks */
int main (int argc, char *argv[]) {
	slog_init(NULL, 99);

	struct json_object *conf = json_load_from_file(argv[1]);
	struct ahttpd_server *server = ahttpd_server_create(conf);
	json_object_put(conf);

    run_script(argv[2], argv[3]);

	aura_handle_events_forever(server->aloop);

	ahttpd_server_destroy(server);
	return 0;
}
