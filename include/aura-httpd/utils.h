#ifndef AURAHTTPD_UTILS_H
#define AURAHTTPD_UTILS_H

#include <aura/aura.h>
#include <aura/list.h>
#include <json.h>
#include <event2/http.h>

#define AHTTPD_FS(s)						   \
        void __attribute__((constructor (101))) do_reg_##s(void) { \
                ahttpd_filesystem_register(&s);			   \
        }

struct ahttpd_mountpoint;

struct ahttpd_server {
	struct evhttp          *eserver;
	struct event_base      *ebase;
	struct aura_eventloop  *aloop;
	char                   *host;
	int                     port;
	struct list_head        mountpoints;
	char 					*index;
	struct hsearch_data    *mimedb;
};

struct ahttpd_fs {
	char *name;
	int   usage;
	int   fsdatalen;
	int (*mount)(struct ahttpd_mountpoint *mpoint);
	void (*unmount)(struct ahttpd_mountpoint *mpoint);
	void (*route)(struct evhttp_request *r, struct ahttpd_mountpoint *mpoint);
	struct list_head qentry;
};

struct ahttpd_mountpoint {
	const char                   *mountpoint;
	json_object            *props;
	struct ahttpd_server   *server;
	const struct ahttpd_fs *fs;
	void                   *fsdata;
	struct list_head        qentry;
};


struct nodefs_data {
	struct aura_node *	node;
	struct json_object *	etable;
	struct list_head	pending_call_list;
	struct list_head	gc_call_list;
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
	struct event *			devt;
};


struct ahttpd_pending_call *ahttpd_call_create(struct ahttpd_mountpoint *mpoint,
					       struct evhttp_request *request,
					       struct aura_object *o, struct json_object *args,
					       int is_async);
void ahttpd_call_delete(struct ahttpd_pending_call *res);


void ahttpd_filesystem_register(struct ahttpd_fs *fs);
int ahttpd_mount(struct ahttpd_server *server, json_object *opts);
void ahttpd_unmount(struct ahttpd_mountpoint *mp);
int ahttpd_method_allowed(struct evhttp_request *request, enum evhttp_cmd_type tp);
void ahttpd_redirect_cb(struct evhttp_request *request, void *arg);

struct json_object *json_find(json_object *arr, char *k);
const char *json_find_string(json_object *o, char *k);


void ahttpd_add_path(struct ahttpd_mountpoint *mpoint, const char *path,
		     void (*cb)(struct evhttp_request *request, void *privParams), void *arg);

void ahttpd_del_path(struct ahttpd_mountpoint *mpoint, const char *path);

void ahttpd_reply_with_json(struct evhttp_request *request, json_object *o);
void ahttpd_reply_accepted(struct evhttp_request *request, const char *redir);

//FixMe: Move these out
void ahttpd_mount_control(struct event_base *ebase, struct evhttp *eserver, json_object *opts);
void ahttpd_mount_node(struct event_base *ebase, struct evhttp *eserver, json_object *opts);


/* JSON SERDES */
json_object *ahttpd_format_to_json(const char *fmt);
json_object *ahttpd_buffer_to_json(struct aura_buffer *buf, const char *fmt);
int ahttpd_buffer_from_json(struct aura_buffer *	buf,
		     struct json_object *	json,
		     const char *		fmt);

const char *ahttpd_mime_guess(struct hsearch_data *instance, const char *filename);
struct hsearch_data *ahttpd_mime_init();
void ahttpd_mime_destroy(struct hsearch_data *instance);

/* server */

struct ahttpd_server *ahttpd_server_create(struct json_object *config);
void ahttpd_server_destroy(struct ahttpd_server *);
struct json_object *json_load_from_file(char *filename);

#endif
