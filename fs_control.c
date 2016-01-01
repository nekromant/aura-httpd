#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>
	
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

static void cfs_mount(struct ahttpd_mountpoint *mpoint)
{
	ahttpd_add_path(mpoint, "/version", version, mpoint);
	ahttpd_add_path(mpoint, "/fstab", fstab, mpoint);
}

static void cfs_unmount(struct ahttpd_mountpoint *mpoint)
{
	ahttpd_del_path(mpoint, "/version");
	ahttpd_del_path(mpoint, "/fstab");
}

static struct ahttpd_fs controlfs =
{
	.name = "control",
	.mount = cfs_mount,
	.unmount = cfs_unmount, 
};

AHTTPD_FS(controlfs);
