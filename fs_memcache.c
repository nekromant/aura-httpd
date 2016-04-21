#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>

struct mcache_data {
//	struct
	int a;
};

static int mfs_mount(struct ahttpd_mountpoint *mpoint)
{
	return 0;
}

static void mfs_unmount(struct ahttpd_mountpoint *mpoint)
{

}

static struct ahttpd_fs memcachefs =
{
	.name = "memcache",
	.mount = mfs_mount,
	.unmount = mfs_unmount,
};
AHTTPD_FS(memcachefs);
