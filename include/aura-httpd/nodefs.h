#ifndef NODEFS_H
#define NODEFS_H

struct nodefs_data {
	struct list_head	pending_call_list;
	struct list_head	gc_call_list;
	struct aura_node *	node;
	struct json_object *	etable;
	int			callid;
};

struct ahttpd_pending_call {
	struct list_head		qentry;
	int				callid;
	struct aura_object *		o;
	char *				path;
	struct ahttpd_mountpoint *	mp;
	int is_async;
	struct evhttp_request *request;
	/* Return values from aura */
	struct json_object *		retbuf;
	const char *			resource_status; /* pending or dead ? */
};

struct ahttpd_pending_call *ahttpd_call_create(struct ahttpd_mountpoint *mpoint,
					       struct evhttp_request *request,
					       struct aura_object *o, struct json_object *args,
					       int is_async);
void ahttpd_call_delete(struct ahttpd_pending_call *res);
void ahttpd_callresource_cleanup(struct ahttpd_mountpoint *mpoint);

json_object *ahttpd_format_to_json(const char *fmt);
json_object *ahttpd_buffer_to_json(struct aura_buffer *buf, const char *fmt);
int ahttpd_buffer_from_json(struct aura_buffer *	buf,
		     struct json_object *	json,
		     const char *		fmt);



#endif /* end of include guard: NODEFS_H */
