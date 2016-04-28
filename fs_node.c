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
 *      /call/name - issue a call
 *      /status - poll for call status
 *      /exports - etable in json
 */


/* Lifetime of a call:
 *      POST at /call/name returns numeric id and starts the call
 *      GET at /result/id
 */



static json_object *object_to_json(const struct aura_object *o)
{
	struct json_object *ret = json_object_new_object();

	if (!ret)
		return NULL;

	struct json_object *name = json_object_new_string(o->name);
	if (!name)
		goto err_no_mem;

	json_object_object_add(ret, "name", name);

	struct json_object *id = json_object_new_int(o->id);
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
		struct json_object *afmt = ahttpd_format_to_json(o->arg_fmt);
		if (!afmt)
			goto err_no_mem;
		json_object_object_add(ret, "afmt", afmt);
	}

	struct json_object *rfmt = ahttpd_format_to_json(o->ret_fmt);
	if (!rfmt)
		goto err_no_mem;

	json_object_object_add(ret, "rfmt", rfmt);

	return ret;
err_no_mem:
	slog(0, SLOG_ERROR, "Failed to serialize object to json");
	json_object_put(ret);
	return NULL;
}

//void call_resource_delete_cb(int fd, short event, void *arg)
//{
//	struct ahttpd_pending_call *res = arg;
//	slog(4, SLOG_DEBUG, "Garbage-collecting resource %s", res->path);
//	call_resource_delete(res);
//}

static struct json_object *extract_json_from_request(struct evhttp_request *request)
{
	char *jsonargs = NULL;
	const struct evhttp_uri *uri = evhttp_request_get_evhttp_uri(request);
	enum evhttp_cmd_type tp =  evhttp_request_get_command(request);

	if (tp == EVHTTP_REQ_GET) {
		const char *query = evhttp_uri_get_query(uri);
		if (!query)
			return NULL;
		jsonargs = evhttp_uridecode(query, 1, NULL);
	} else if (tp == EVHTTP_REQ_POST) {
		/* TODO: ... */
	}

	if (!jsonargs)
		return NULL;

	enum json_tokener_error error = json_tokener_success;
	json_object *ret = json_tokener_parse_verbose(jsonargs, &error);
	free(jsonargs);
	if (error != json_tokener_success)
		return NULL;
	return ret;
}

static void issue_call(struct evhttp_request *request,
					   struct ahttpd_mountpoint *mpoint,
				       const char *name, int is_async)
{

	struct nodefs_data *nd = mpoint->fsdata;
	struct aura_node *node = nd->node;
	struct aura_object *o;

	if (!ahttpd_method_allowed(request, EVHTTP_REQ_GET | EVHTTP_REQ_PUT))
		return;

	struct json_object *args = extract_json_from_request(request);
	if (!args) {
		evhttp_send_error(request, 400, "Failed to extract request data");
		return;
	}

	o = aura_etable_find(node->tbl, name);
	if (!o) {
		evhttp_send_error(request, 400, "Failed to locate find method in etable");
		return;
	}

	ahttpd_call_create(mpoint, request, o, args, is_async);
}

static const char *get_call_methodname(struct ahttpd_mountpoint *mpoint, struct evhttp_request *request, char *basepath)
{
	const struct evhttp_uri *uri = evhttp_request_get_evhttp_uri(request);
	const char *name = evhttp_uri_get_path(uri);
	name = &name[strlen(mpoint->mountpoint) + strlen("/call/")];
	return name;
}

static void issue_call_async_cb(struct evhttp_request *request, void *arg)
{
	struct ahttpd_mountpoint *mpoint = arg;
	const char *name = get_call_methodname(mpoint, request, "/acall/");
	issue_call(request, mpoint, name, 1);
}

static void issue_call_sync_cb(struct evhttp_request *request, void *arg)
{
	struct ahttpd_mountpoint *mpoint = arg;
	const char *name = get_call_methodname(mpoint, request, "/call/");
	issue_call(request, mpoint, name, 0);
}


static void callpath_add(const struct aura_object *o, struct ahttpd_mountpoint *mpoint)
{
	char *callpath;
	struct nodefs_data *nd = mpoint->fsdata;

	if (-1 == asprintf(&callpath, "/call/%s", o->name))
		BUG(nd->node, "Out of memory");
	ahttpd_add_path(mpoint, callpath, issue_call_sync_cb, mpoint);
	free(callpath);

	if (-1 == asprintf(&callpath, "/acall/%s", o->name))
		BUG(nd->node, "Out of memory");
	ahttpd_add_path(mpoint, callpath, issue_call_async_cb, mpoint);
	free(callpath);
}

static void callpath_delete(const struct aura_object *o, struct ahttpd_mountpoint *mpoint)
{
	char *callpath;

	if (-1 == asprintf(&callpath, "/call/%s", o->name))
		BUG(NULL, "Out of memory");
	ahttpd_del_path(mpoint, callpath);
	free(callpath);
}

static void etable_delete_callpaths(struct ahttpd_mountpoint *		mpoint,
				    const struct aura_export_table *	old)
{
	int i;
	struct nodefs_data *nd = mpoint->fsdata;

	if (nd->etable)
		json_object_put(nd->etable);

	if (!old)
		return;

	for (i = 0; i < old->next; i++)
		callpath_delete(&old->objects[i], mpoint);
}

static void etable_create_callpaths(struct ahttpd_mountpoint *		mpoint,
				    const struct aura_export_table *	new)
{
	int i;
	struct nodefs_data *nd = mpoint->fsdata;

	if (!new)
		return;

	nd->etable = json_object_new_array();
	for (i = 0; i < new->next; i++) {
		const struct aura_object *o = &new->objects[i];
		struct json_object *obj = object_to_json(o);
		callpath_add(o, mpoint);
		json_object_array_add(nd->etable, obj);
	}
}



static void etbl_changed_cb(struct aura_node *		node,
			    struct aura_export_table *	old,
			    struct aura_export_table *	new,
			    void *			arg)
{
	struct ahttpd_mountpoint *mpoint = arg;

	slog(4, SLOG_DEBUG, "Aura reports etable change - propagating");
	etable_delete_callpaths(mpoint, old);
	etable_create_callpaths(mpoint, new);
}

static void exports(struct evhttp_request *request, void *arg)
{
	struct nodefs_data *nd = arg;

	ahttpd_reply_with_json(request, nd->etable);
}

static void status(struct evhttp_request *request, void *arg)
{
	struct nodefs_data *nd = arg;
	struct json_object *reply = json_object_new_object();
	struct json_object *jstatus;
	int status = aura_get_status(nd->node);

	if (status == AURA_STATUS_ONLINE)
		jstatus = json_object_new_string("online");
	else
		jstatus = json_object_new_string("offline");
	json_object_object_add(reply, "status", jstatus);
	/* TODO: Reply with more info */
	ahttpd_reply_with_json(request, reply);
	json_object_put(reply);
}

static void events(struct evhttp_request *request, void *arg)
{
	struct nodefs_data *nd = arg;
	struct json_object *list = json_object_new_array();
	int i;

	for (i = 0; i < aura_get_pending_events(nd->node); i++) {
		struct json_object *evt = json_object_new_object();
		struct aura_buffer *buf;
		const struct aura_object *o;
		struct json_object *jo;
		struct json_object *ji;
		struct json_object *jd;

		json_object_array_add(list, evt);
		aura_get_next_event(nd->node, &o, &buf);
		jo = json_object_new_string(o->name);
		ji = json_object_new_int(o->id);
		jd = ahttpd_buffer_to_json(buf, o->ret_fmt);
		json_object_object_add(evt, "id", ji);
		json_object_object_add(evt, "name", jo);
		json_object_object_add(evt, "data", jd);
		aura_buffer_release(buf);
	}
	ahttpd_reply_with_json(request, list);
	json_object_put(list);
}


static int node_mount(struct ahttpd_mountpoint *mpoint)
{
	const char *mp = mpoint->mountpoint;
	struct nodefs_data *nd = mpoint->fsdata;

	const char *tr = json_find_string(mpoint->props, "transport");
	const char *options = json_find_string(mpoint->props, "options");

	if (!tr || !options) {
		slog(0, SLOG_WARN, "Not mounting node: missing transport name or params");
		return -1;
	}

	nd->node = aura_open(tr, options);
	if (!nd->node) {
		slog(0, SLOG_ERROR, "Failed to open node %s @ %s", tr, mp);
		return -1;
	}

	nd->etable = NULL;
	aura_set_userdata(nd->node, mpoint);
	aura_eventloop_add(mpoint->server->aloop, nd->node);
	aura_etable_changed_cb(nd->node, etbl_changed_cb, mpoint);
	aura_enable_sync_events(nd->node, 100); /* TODO: Move to config */
	ahttpd_add_path(mpoint, "/exports", exports, nd);
	ahttpd_add_path(mpoint, "/events", events, nd);
	ahttpd_add_path(mpoint, "/status", status, nd);
	INIT_LIST_HEAD(&nd->pending_call_list);
	INIT_LIST_HEAD(&nd->gc_call_list);
	return 0;
}

static void node_unmount(struct ahttpd_mountpoint *mpoint)
{
	struct nodefs_data *nd = mpoint->fsdata;

	ahttpd_del_path(mpoint, "/exports");
	ahttpd_del_path(mpoint, "/events");
	ahttpd_del_path(mpoint, "/status");

	if (nd->node)
		aura_close(nd->node);
}

static struct ahttpd_fs nodefs =
{
	.name		= "node",
	.mount		= node_mount,
	.unmount	= node_unmount,
	.fsdatalen	= sizeof(struct nodefs_data),
};

AHTTPD_FS(nodefs);
