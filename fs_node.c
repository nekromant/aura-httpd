#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>

struct nodefs_data {
	struct aura_node *node;
};

static void etbl_changed_cb(struct aura_node *node, struct aura_export_table *old, struct aura_export_table *new, void *arg)
{
	slog(4, SLOG_DEBUG, "Etable changed - propagating");
	
}

void fd_changed_cb(const struct aura_pollfds *fd, enum aura_fd_action act, void *arg)
{
	slog(4, SLOG_DEBUG, "Descriptor change event %d", act);
}

static void node_mount(struct ahttpd_mountpoint *mpoint)
{
	const char *mp      = mpoint->mountpoint;
	const char *tr      = json_find_string(mpoint->props, "transport");
	const char *options = json_find_string(mpoint->props, "options");
	
	if (!tr || !options) { 
		slog(0, SLOG_WARN, "Not mounting node: missing transport name or params");
		return;
	}
	
	struct aura_node *node = aura_open(tr, options);
	if (!node) {
		slog(0, SLOG_ERROR, "Failed to open node %s @ %s\n", tr, mp);
		exit(1);
	}

	aura_fd_changed_cb(node, fd_changed_cb, mpoint);

	if (!mpoint->server->aloop) { 
		slog(1, SLOG_DEBUG, "Creating aura eventsystem for server");
		mpoint->server->aloop = aura_eventloop_create(node);
		if (!mpoint->server->aloop)
			BUG(node, "Eventloop creation failed");
	}
}

static void node_unmount(struct ahttpd_mountpoint *mpoint)
{
	
}

static struct ahttpd_fs nodefs =
{
	.name = "node",
	.mount = node_mount,
	.unmount = node_unmount, 
	.fsdatalen = sizeof(struct nodefs_data),
};

AHTTPD_FS(nodefs);
