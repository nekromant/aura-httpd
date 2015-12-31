#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>


static void etbl_changed_cb(struct aura_node *node, struct aura_export_table *old, struct aura_export_table *new, void *arg)
{
	slog(4, SLOG_DEBUG, "Etable changed - propagating");	
}

void ahttpd_mount_node(struct event_base *ebase, struct evhttp *eserver, json_object *opts)
{
	const char *mp      = json_find_string(opts, "mountpoint");
	const char *tr      = json_find_string(opts, "transport");
	const char *options = json_find_string(opts, "options");
	
	if (!tr || !options) { 
		slog(0, SLOG_WARN, "Not mounting node: missing transport name or params");
		return;
	}
	
	struct aura_node *node = aura_open(tr, options);
	if (!node) {
		slog(0, SLOG_ERROR, "Failed to open node %s @ %s\n", tr, mp);
		exit(1);
	}
	aura_etable_changed_cb(node, etbl_changed_cb, eserver);	
}
