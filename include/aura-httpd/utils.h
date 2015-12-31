#ifndef AURAHTTPD_UTILS_H
#define AURAHTTPD_UTILS_H

#include <aura/list.h>

struct json_object *json_find(json_object *arr, char *k);
const char *json_find_string(json_object *o, char *k);
void ahttpd_mount(struct evhttp *eserver, const char *mountpoint, const char *path, 
		  void (*cb)(struct evhttp_request *request, void *privParams), void *arg);
void ahttpd_reply_with_json(struct evhttp_request *request, json_object *o);


//FixMe: Move these out
void ahttpd_mount_control(struct event_base *ebase, struct evhttp *eserver, json_object *opts);
void ahttpd_mount_node(struct event_base *ebase, struct evhttp *eserver, json_object *opts);

#endif
