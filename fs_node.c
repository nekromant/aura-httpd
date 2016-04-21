#include <aura/aura.h>
#include <aura/private.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>

/*
	/call/name - issue a call
	/status - poll for call status
	/exports - etable in json
 */

struct nodefs_data {
	struct aura_node *node;
	struct json_object *etable;
};

static json_object *format_to_json(const char *fmt)
{
	int len;
	struct json_object *ret = json_object_new_array();

	if (!fmt)
		return ret;

	while (*fmt) {
		struct json_object *tmp;
		switch (*fmt++) {
		case URPC_U8:
			tmp = json_object_new_string("uint8_t");
			break;
		case URPC_U16:
			tmp = json_object_new_string("uint16_t");
			break;
		case URPC_U32:
			tmp = json_object_new_string("uint32_t");
			break;
		case URPC_U64:
			tmp = json_object_new_string("uint64_t");
			break;

		case URPC_S8:
			tmp = json_object_new_string("int8_t");
			break;
		case URPC_S16:
			tmp = json_object_new_string("int16_t");
			break;
		case URPC_S32:
			tmp = json_object_new_string("int32_t");
			break;
		case URPC_S64:
			tmp = json_object_new_string("int64_t");
			break;
		case URPC_BUF:
			tmp = json_object_new_string("buffer");
			break;
		case URPC_BIN:
			len = atoi(fmt);
			tmp = json_object_new_object();
			struct json_object *ln = NULL;
			if (len == 0) {
				BUG(NULL, "Internal serializer bug processing: %s", fmt);
			} else {
				ln = json_object_new_int(len);
			}
			if (!ln || !tmp)
				BUG(NULL, "Out of memory while serializing json");
			json_object_object_add(tmp, "binary", ln);
			while (*fmt && (*fmt++ != '.'));
			break;
		case 0x0:
			tmp = json_object_new_object();
			break;
		}
		json_object_array_add(ret, tmp);
	}
	return ret;
}

static json_object *object_to_json(struct aura_object *o, int idx)
{
	struct json_object *ret = json_object_new_object();
	if (!ret)
		return NULL;

	struct json_object *name = json_object_new_string(o->name);
	if (!name)
		goto err_no_mem;

	json_object_object_add(ret, "name", name);

	struct json_object *id = json_object_new_int(idx);
	if (!id)
		goto err_no_mem;

	json_object_object_add(ret, "id", id);

	struct json_object *type;
	if (object_is_event(o))
		type = json_object_new_string("event");
	else
		type = json_object_new_string("method");

	if (!type)
		goto err_no_mem;

	json_object_object_add(ret, "type", type);

	if (object_is_method(o)) {
		struct json_object *afmt = format_to_json(o->arg_fmt);
		if (!afmt)
			goto err_no_mem;
		json_object_object_add(ret, "afmt", afmt);
	}

	struct json_object *rfmt = format_to_json(o->ret_fmt);
	if (!rfmt)
		goto err_no_mem;

	json_object_object_add(ret, "rfmt", rfmt);

	return ret;
err_no_mem:
	slog(0, SLOG_ERROR, "Failed to serialize object to json");
	json_object_put(ret);
	return NULL;
}

static void etbl_changed_cb(struct aura_node *node,
	struct aura_export_table *old,
	struct aura_export_table *new,
	void *arg)
{
	struct ahttpd_mountpoint *mpoint = arg;
	struct nodefs_data *nd           = mpoint->fsdata;
	slog(4, SLOG_DEBUG, "Aura reports etable change - propagating");
	if (nd->etable)
		json_object_put(nd->etable);
	nd->etable = json_object_new_array();
	int i;
	for (i=0; i < new->next; i++) {
		struct aura_object *o = &new->objects[i];
		struct json_object *obj = object_to_json(o, i);
		json_object_array_add(nd->etable, obj);
	}
}

static void exports(struct evhttp_request *request, void *arg)
{
	struct nodefs_data *nd = arg;
	ahttpd_reply_with_json(request, nd->etable);
}

static int node_mount(struct ahttpd_mountpoint *mpoint)
{
	const char *mp         = mpoint->mountpoint;
	struct nodefs_data *nd = mpoint->fsdata;

	const char *tr         = json_find_string(mpoint->props, "transport");
	const char *options    = json_find_string(mpoint->props, "options");

	if (!tr || !options) {
		slog(0, SLOG_WARN, "Not mounting node: missing transport name or params");
		return -1;
	}

	nd->node = aura_open(tr, options);
	if (!nd->node) {
		slog(0, SLOG_ERROR, "Failed to open node %s @ %s\n", tr, mp);
		return -1;
	}
	nd->etable = NULL;

	aura_eventloop_add(mpoint->server->aloop, nd->node);
	aura_etable_changed_cb(nd->node, etbl_changed_cb, mpoint);
	ahttpd_add_path(mpoint, "/exports", exports, nd);
	return 0;
}

static void node_unmount(struct ahttpd_mountpoint *mpoint)
{
	struct nodefs_data *nd = mpoint->fsdata;
	aura_close(nd->node);
	json_object_put(nd->etable);
}

static struct ahttpd_fs nodefs =
{
	.name = "node",
	.mount = node_mount,
	.unmount = node_unmount,
	.fsdatalen = sizeof(struct nodefs_data),
};

AHTTPD_FS(nodefs);
