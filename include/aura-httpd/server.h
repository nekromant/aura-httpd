#ifndef AURAHTTPD_UTILS_H
#define AURAHTTPD_UTILS_H

#include <aura/aura.h>
#include <aura/list.h>
#include <json.h>
#include <event2/http.h>

struct ahttpd_mountpoint;

struct ahttpd_server {
	struct evhttp          *eserver;
	struct event_base      *ebase;
	struct aura_eventloop  *aloop;
	char                   *host;
	char 				   *name;
	int                     port;
	struct list_head        mountpoints;
	char 					*index;
	struct hsearch_data    *mimedb;
	long max_body_size; /* TODO: init default */
	long max_headers_size;
};


int ahttpd_method_allowed(struct evhttp_request *request, enum evhttp_cmd_type tp);
void ahttpd_redirect_cb(struct evhttp_request *request, void *arg);


struct json_object *json_array_find(json_object *arr, char *k);
const char *json_array_find_string(json_object *o, char *k);
long json_array_find_number(json_object *o, char *k);
bool json_array_find_boolean(json_object *o, char *k);

void ahttpd_add_path(struct ahttpd_mountpoint *mpoint, const char *path,
		     void (*cb)(struct evhttp_request *request, void *privParams), void *arg);

void ahttpd_del_path(struct ahttpd_mountpoint *mpoint, const char *path);

void ahttpd_reply_with_json(struct evhttp_request *request, json_object *o);
void ahttpd_reply_accepted(struct evhttp_request *request, const char *redir);

//FixMe: Move these out
void ahttpd_mount_control(struct event_base *ebase, struct evhttp *eserver, json_object *opts);
void ahttpd_mount_node(struct event_base *ebase, struct evhttp *eserver, json_object *opts);


/* JSON SERDES */
struct hsearch_data *ahttpd_mime_init();
void ahttpd_mime_destroy(struct hsearch_data *instance);
const char *ahttpd_mime_guess(struct hsearch_data *instance, const char *filename);

/* server */

struct ahttpd_server *ahttpd_server_create(struct json_object *config);
void ahttpd_server_destroy(struct ahttpd_server *);
struct json_object *json_load_from_file(const char *filename);

#endif
