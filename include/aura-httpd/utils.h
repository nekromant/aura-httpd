#ifndef AURAHTTPD_UTILS_H
#define AURAHTTPD_UTILS_H

#include <aura/list.h>

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
};

struct ahttpd_fs {
	char *name;
	int   usage;
	int   fsdatalen;
	void (*mount)(struct ahttpd_mountpoint *mpoint);
	void (*unmount)(struct ahttpd_mountpoint *mpoint);
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


void ahttpd_filesystem_register(struct ahttpd_fs *fs);
int ahttpd_mount(struct ahttpd_server *server, json_object *opts);

struct json_object *json_find(json_object *arr, char *k);
const char *json_find_string(json_object *o, char *k);


void ahttpd_add_path(struct ahttpd_mountpoint *mpoint, const char *path, 
		     void (*cb)(struct evhttp_request *request, void *privParams), void *arg);

void ahttpd_del_path(struct ahttpd_mountpoint *mpoint, const char *path);

void ahttpd_reply_with_json(struct evhttp_request *request, json_object *o);

//FixMe: Move these out
void ahttpd_mount_control(struct event_base *ebase, struct evhttp *eserver, json_object *opts);
void ahttpd_mount_node(struct event_base *ebase, struct evhttp *eserver, json_object *opts);

#endif
