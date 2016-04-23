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

struct pending_call_resource {
	struct list_head	qenty;
	int			callid;
	struct aura_object *	o;
};

struct nodefs_data {
	struct aura_node *	node;
	struct json_object *	etable;
	struct list_head	pending_call_list;
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
			if (len == 0)
				BUG(NULL, "Internal serializer bug processing: %s", fmt);
			else
				ln = json_object_new_int(len);
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

static json_object *buffer_to_json(struct aura_buffer *buf, const char *fmt)
{
	struct json_object *ret = json_object_new_array();

	if (!ret)
		return NULL;

	union {
		uint8_t u8;
		uint8_t u16;
		uint8_t u32;
		uint8_t u64;
		int8_t	s8;
		int8_t	s16;
		int8_t	s32;
		int8_t	s64;
	} var;

#define FETCH_VAR_IN_CASE(cs, sz) \
case cs: \
	var.sz = aura_buffer_get_ ## sz(buf); \
	tmp = json_object_new_int(var.sz); \
	break;

	if (!fmt)
		return ret;
	while (*fmt) {
		int len;
		slog(0, SLOG_DEBUG, "FMT %s", fmt);
		struct json_object *tmp = NULL;
		switch (*fmt++) {
			FETCH_VAR_IN_CASE(URPC_U8, u8);
			FETCH_VAR_IN_CASE(URPC_S8, s8);
			FETCH_VAR_IN_CASE(URPC_U16, u16);
			FETCH_VAR_IN_CASE(URPC_S16, s16);
			FETCH_VAR_IN_CASE(URPC_U32, u32);
			FETCH_VAR_IN_CASE(URPC_S32, s32);
			FETCH_VAR_IN_CASE(URPC_U64, u64);
			FETCH_VAR_IN_CASE(URPC_S64, s64);

		case URPC_BUF:
			//FixMe: TODO:...
			tmp = json_object_new_string("buffer");
			break;
		case URPC_BIN:
			len = atoi(fmt);
			tmp = json_object_new_string("buffer");
			while (*fmt && (*fmt++ != '.')) ;
			break;
		case 0x0:
			tmp = json_object_new_object();
			break;
		}

		if (!tmp)
			BUG(NULL, "WTF?");
		json_object_array_add(ret, tmp);
		tmp = NULL;
	}
	return ret;
}

int buffer_from_json(struct aura_buffer *	buf,
		     struct json_object *	json,
		     const char *		fmt)
{
	int i=0;
	if (!json_object_is_type(json, json_type_array))
		return -1;

	if (!fmt)
		return 0; /* All done ;) */

	union {
		uint8_t u8;
		uint8_t u16;
		uint8_t u32;
		uint8_t u64;
		int8_t	s8;
		int8_t	s16;
		int8_t	s32;
		int8_t	s64;
	} var;

#define PUT_VAR_IN_CASE(cs, sz) \
case cs: \
	tmp = json_object_array_get_idx(json, i); \
	if (!json_object_is_type(json, json_type_int)) \
		return -1; \
	var.sz = json_object_get_int64(tmp); \
	aura_buffer_put_ ## sz(buf, var.sz); \
	break;

	while (*fmt) {
		int len;
		slog(0, SLOG_DEBUG, "FMT %s", fmt);
		struct json_object *tmp = NULL;
		switch (*fmt++) {
			PUT_VAR_IN_CASE(URPC_U8, u8);
			PUT_VAR_IN_CASE(URPC_S8, s8);
			PUT_VAR_IN_CASE(URPC_U16, u16);
			PUT_VAR_IN_CASE(URPC_S16, s16);
			PUT_VAR_IN_CASE(URPC_U32, u32);
			PUT_VAR_IN_CASE(URPC_S32, s32);
			PUT_VAR_IN_CASE(URPC_U64, u64);
			PUT_VAR_IN_CASE(URPC_S64, s64);

		case URPC_BUF:
			//FixMe: TODO:...
			BUG(NULL, "Not implemented");
			break;
		case URPC_BIN:
			len = atoi(fmt);
			BUG(NULL, "Not implemented");
			while (*fmt && (*fmt++ != '.')) ;
			break;
		case 0x0:
			BUG(NULL, "Not implemented");
			break;
		}
	}
	return 0;
}

static void issue_call(struct evhttp_request *request, void *arg)
{
	struct aura_object *o = arg;
	ahttpd_allow_method(request, EVHTTP_REQ_POST);
	
}

static void callpath_add(const struct aura_object *o, struct ahttpd_mountpoint *mpoint)
{
	char *callpath;
	struct nodefs_data *nd = mpoint->fsdata;
	if (-1 == asprintf(&callpath, "/call/%s", o->name))
		BUG(nd->node, "Out of memory");
	ahttpd_add_path(mpoint, callpath, issue_call, (void *) o);
	free(callpath);
}

static void callpath_delete(const struct aura_object *o, struct ahttpd_mountpoint *mpoint)
{
	char *callpath;
	if (-1 == asprintf(&callpath, "/call/%s", o->name))
		BUG(NULL, "Out of memory");
	ahttpd_add_path(mpoint, callpath, issue_call, (void *) o);
	free(callpath);
}

static void etbl_changed_cb(struct aura_node *		node,
			    struct aura_export_table *	old,
			    struct aura_export_table *	new,
			    void *			arg)
{
	struct ahttpd_mountpoint *mpoint = arg;
	struct nodefs_data *nd = mpoint->fsdata;
	int i;

	slog(4, SLOG_DEBUG, "Aura reports etable change - propagating");
	if (nd->etable)
		json_object_put(nd->etable);

	if (old)
		for (i = 0; i < old->next; i++)
			callpath_delete(&old->objects[i], mpoint);

	nd->etable = json_object_new_array();

	for (i = 0; i < new->next; i++) {
		struct aura_object *o = &new->objects[i];
		struct json_object *obj = object_to_json(o);
		callpath_add(o, mpoint);
		json_object_array_add(nd->etable, obj);
	}
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
		jd = buffer_to_json(buf, o->ret_fmt);
		json_object_object_add(evt, "id", ji);
		json_object_object_add(evt, "name", jo);
		json_object_object_add(evt, "data", jd);
		aura_buffer_release(buf);
	}
	ahttpd_reply_with_json(request, list);
	json_object_put(list);
}

static void call(struct evhttp_request *request, void *arg)
{
	struct nodefs_data *nd = arg;

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
		slog(0, SLOG_ERROR, "Failed to open node %s @ %s\n", tr, mp);
		return -1;
	}
	nd->etable = NULL;

	aura_eventloop_add(mpoint->server->aloop, nd->node);
	aura_etable_changed_cb(nd->node, etbl_changed_cb, mpoint);
	aura_enable_sync_events(nd->node, 100); /* TODO: Move to config */
	ahttpd_add_path(mpoint, "/exports", exports, nd);
	ahttpd_add_path(mpoint, "/events", events, nd);
	ahttpd_add_path(mpoint, "/status", status, nd);

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
	.name		= "node",
	.mount		= node_mount,
	.unmount	= node_unmount,
	.fsdatalen	= sizeof(struct nodefs_data),
};

AHTTPD_FS(nodefs);
