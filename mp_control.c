#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>
	

static void version(struct evhttp_request *request, void *privParams)
{
	json_object *root = json_object_new_object();
	json_object *ver =  json_object_new_string(aura_get_version());
	json_object *code = json_object_new_int(aura_get_version_code());
	json_object_object_add(root, "aura_version", ver);
	json_object_object_add(root, "aura_versioncode", code);
	ahttpd_reply_with_json(request, root);
}

void ahttpd_mount_control(struct event_base *ebase, struct evhttp *eserver, json_object *opts)
{
	const char *mp = json_find_string(opts, "mountpoint");
	slog(1, SLOG_INFO, "Mounting control interface at %s", mp);
	ahttpd_mount(eserver, mp, "/version", version, NULL);
}
