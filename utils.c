#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>


struct json_object *json_find(json_object *arr, char *k)
{
	json_object_object_foreach(arr, key, val) {
		if (strcmp(key, k) == 0)
			return val;
	}
	return NULL;
}

const char *json_find_string(json_object *o, char *k)
{
	json_object *tmp = json_find(o, "mountpoint");
	if (!tmp)
		return NULL;
	return json_object_get_string(tmp); 	
}

void ahttpd_mount(struct evhttp *eserver, const char *mountpoint, const char *path, 
		  void (*cb)(struct evhttp_request *request, void *privParams), void *arg)
{
	char *str; 
	// TODO: Trailing slashes!
	int ret = asprintf(&str, "%s/%s", mountpoint, path);
	if (ret == -1)
		BUG(NULL, "asprintf() failed!");

	evhttp_set_cb (eserver, str, cb, arg);
	free(str);	
}

void ahttpd_reply_with_json(struct evhttp_request *request, json_object *o)
{
	struct evbuffer *buffer = evbuffer_new();
	const char *str = json_object_to_json_string(o);
	evbuffer_add(buffer, str, strlen(str)); 
	evhttp_add_header (evhttp_request_get_output_headers (request),
			"Content-Type", "text/plain");
	evhttp_send_reply(request, HTTP_OK, "OK", buffer);
	evbuffer_free (buffer);
}
