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

void conn_close_cb(struct evhttp_connection *con, void *arg)
{
	struct ahttpd_pending_call *res = arg;
	slog(0, SLOG_DEBUG, "GC Temporary resource %s", res->path);
	ahttpd_call_delete(res);
}

static void resource_readout(struct evhttp_request *request, void *arg)
{
	struct ahttpd_pending_call *res = arg;

	slog(4, SLOG_DEBUG, "Readout : %s", res->path);
	struct nodefs_data *nd = res->mp->fsdata;
	if (res->retbuf) { /* Call completed already */
		ahttpd_reply_with_json(request, res->retbuf);
		/* We can't call evhttp_del_cb here, so we move resource to our
		 * gc list to be freed later */
		list_del(&res->qentry);
		list_add_tail(&res->qentry, &nd->gc_call_list);
		/* In the rare case something reads it before we've deleted the path */
		res->resource_status = "dead";
		/* And schedule resource deletion once connection's closed */
		struct evhttp_connection *con = evhttp_request_get_connection(request);
		if (!con)
			BUG(NULL, "WTF?");
		evhttp_connection_set_closecb(con, conn_close_cb, res);
	} else {
		struct json_object *tmp = json_object_new_object();
		struct json_object *result_json = json_object_new_string(res->resource_status);
		json_object_object_add(tmp, "status", result_json);
		ahttpd_reply_with_json(request, tmp);
		json_object_put(tmp);
	}
}

struct json_object *call_result_to_json(int result, struct aura_object *o,
	struct aura_buffer *retbuf)
{
	struct json_object *ret = json_object_new_object();
	if (!ret)
		BUG(retbuf->owner, "JSON object allocation failed");
	struct json_object *result_json = json_object_new_string("completed");
	json_object_object_add(ret, "status", result_json);
	if (retbuf) {
		struct json_object *retbuf_json = ahttpd_buffer_to_json(retbuf, o->ret_fmt);
		json_object_object_add(ret, "data", retbuf_json);
	}
	return ret;
}

void call_completed_cb(struct aura_node *node, int result, struct aura_buffer *retbuf, void *arg)
{
	struct ahttpd_pending_call *res = arg;
	res->retbuf = call_result_to_json(result, res->o, retbuf);
	if (res->is_async)
		return; /* That's all for async */
	/* For sync we need to reply here */
	ahttpd_reply_with_json(res->request, res->retbuf);
	/* And schedule resource deletion */
	struct evhttp_connection *con = evhttp_request_get_connection(res->request);
	if (!con)
		BUG(NULL, "WTF?");
	evhttp_connection_set_closecb(con, conn_close_cb, res);
}


void ahttpd_call_delete(struct ahttpd_pending_call *res)
{
	list_del(&res->qentry);
	evhttp_del_cb(res->mp->server->eserver, res->path);
	json_object_put(res->retbuf);
	free(res->path);
	free(res);
}

struct ahttpd_pending_call *ahttpd_call_create(struct ahttpd_mountpoint *mpoint,
					       struct evhttp_request *request,
					       struct aura_object *o, struct json_object *args,
					       int is_async)
{
	int ret;
	struct nodefs_data *nd = mpoint->fsdata;

	if (strcmp(mpoint->fs->name, "node") != 0)
		BUG(NULL, "Attempt to create a pending call on non-node mountpoint");

	struct ahttpd_pending_call *res = calloc(1, sizeof(*res));

	if (!res)
		BUG(nd->node, "Malloc failed!");

	struct aura_buffer *buf = aura_buffer_request(nd->node, o->arglen);
	ret = ahttpd_buffer_from_json(buf, args, o->arg_fmt);
	if (ret != 0) {
		evhttp_send_error(request, 400, "Problem marshalling data");
		goto bailout;
	}

	res->callid = nd->callid++;
	res->o = o;
	res->resource_status = "pending";
	res->mp = mpoint;
	res->is_async = is_async;
	res->request = request;

	ret = asprintf(&res->path, "%s/pending/%d", mpoint->mountpoint, res->callid);
	if (-1 == ret)
		BUG(nd->node, "Malloc failed");

	ret = aura_core_start_call(nd->node, o, call_completed_cb, res, buf);
	if (ret != 0) {
		slog(0, SLOG_WARN, "aura_core_start_call() failed with %d", ret);
		evhttp_send_error(request, 500, "Failed to start aura call");
		goto bailout;
	}

	/* Allow to readout temporary resource */
	evhttp_set_cb(mpoint->server->eserver, res->path, resource_readout, res);

	slog(4, SLOG_DEBUG, "Created temporary resource %s for %s", res->path, o->name);
	list_add_tail(&res->qentry, &nd->pending_call_list);
	if (is_async)
		ahttpd_reply_accepted(request, res->path);
	return res;
bailout:
	free(res);
	return NULL;
}
