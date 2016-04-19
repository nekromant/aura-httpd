#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>


struct staticfs_data {
	const char *htdocs;
};
	
static void version(struct evhttp_request *request, void *arg)
{
	json_object *root = json_object_new_object();
	json_object *ver =  json_object_new_string(aura_get_version());
	json_object *code = json_object_new_int(aura_get_version_code());
	json_object_object_add(root, "aura_version", ver);
	json_object_object_add(root, "aura_versioncode", code);
	ahttpd_reply_with_json(request, root);
	json_object_put(root);
}

static void fstab(struct evhttp_request *request, void *arg)
{
	struct ahttpd_mountpoint *mpoint = arg;
	json_object *root = json_object_new_object();
	json_object *arr = json_object_new_array();
	struct list_head *h = &mpoint->server->mountpoints;
	struct ahttpd_mountpoint *pos;
        list_for_each_entry(pos, h, qentry) {
		json_object_array_add(arr, pos->props);
		json_object_get(pos->props);
	}
	json_object_object_add(root, "fstab", arr);
	ahttpd_reply_with_json(request, root);
	json_object_put(root);
}

void router(struct evhttp_request *r, struct ahttpd_mountpoint *mpoint) 
{
	char *uri = &evhttp_request_get_uri(r)[strlen(mpoint->mountpoint)];
	//TODO: URI sanity-checking to ward off k00lhax0rz
	char *path;

	if (-1 == asprintf(&path, "%s/%s", "", uri)) {
		evhttp_send_error(r, 500, "We have some serverside fuckup. Sorry."); 
		return;
	}

	evhttp_send_error(r, 500, "We have some serverside fuckup. Sorry."); 
}

static void cfs_mount(struct ahttpd_mountpoint *mpoint)
{
//	evhttp_set_gencb(mpoint->server->eserver, router, mpoint);
//	ahttpd_add_path(mpoint, "/version", version, mpoint);
//	ahttpd_add_path(mpoint, "/fstab", fstab, mpoint);
}

static void cfs_unmount(struct ahttpd_mountpoint *mpoint)
{
//	ahttpd_del_path(mpoint, "/version");
//	ahttpd_del_path(mpoint, "/fstab");
}

static struct ahttpd_fs staticfs =
{
	.name = "static",
	.mount = cfs_mount,
	.unmount = cfs_unmount, 
	.route = router,
	.fsdatalen = sizeof(struct staticfs_data),
};

AHTTPD_FS(staticfs);
