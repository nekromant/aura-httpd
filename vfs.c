#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>

static LIST_HEAD(fslist);

#define required(_rq)                                                   \
        if (!fs->_rq) {                                                 \
        slog(0, SLOG_WARN,                                              \
             "Filesystem %s missing required field aura_transport.%s; Disabled", \
             fs->name, #_rq                                             \
                );                                                      \
        return;                                                         \
        }


void ahttpd_filesystem_register(struct ahttpd_fs *fs)
{
	required(mount);
	required(unmount);
	slog(0, SLOG_INFO, "Registering new filesystem: %s", fs->name);
	list_add_tail(&fs->qentry, &fslist);
}

struct ahttpd_fs *ahttpd_filesystem_lookup(const char *name)
{
	struct ahttpd_fs *pos;
        list_for_each_entry(pos, &fslist, qentry)
                if (strcmp(pos->name, name) == 0 ) {
                        pos->usage++;
                        return pos;
                }
        return NULL;
}

int ahttpd_mount(struct ahttpd_server *server, json_object *opts)
{
	const char *mp = json_find_string(opts, "mountpoint");
	const char *tp = json_find_string(opts, "type");

	if ((tp == NULL) || (mp == NULL))
		BUG(NULL, "Misconfigurated node section, skipping");

	struct ahttpd_fs *fs = ahttpd_filesystem_lookup(tp);
	if (!fs) {
		slog(0, SLOG_WARN, "Filesystem %s (%s) is unknown to us", tp, mp);
		return -1;
	}

	struct ahttpd_mountpoint *mpoint = calloc(1, sizeof(*mpoint));
	if (!mpoint)
		BUG(NULL, "calloc() failed!");

	mpoint->fs = fs;

	if (fs->fsdatalen)
		mpoint->fsdata = calloc(1, fs->fsdatalen);

	if ((fs->fsdatalen) && (!mpoint->fsdata))
		BUG(NULL, "calloc() of fsdata failed!");

	mpoint->server = server;
	mpoint->mountpoint = mp;
	mpoint->props = opts;

	json_object_get(opts);

	slog(1, SLOG_INFO, "Mounting %s at %s", tp, mp);
	if (fs->mount(mpoint) == 0)
		list_add_tail(&mpoint->qentry, &server->mountpoints);
	else {
		if (mpoint->fsdata)
			free(mpoint->fsdata);
		free(mpoint);
	}

	return 0;
}

void ahttpd_unmount(struct ahttpd_mountpoint *mp)
{
	slog(4, SLOG_DEBUG, "Unmounting %s", mp->mountpoint);
	mp->fs->unmount(mp);
	json_object_put(mp->props);
	list_del(&mp->qentry);
	if (mp->fsdata)
		free(mp->fsdata);

	free(mp);
}


void ahttpd_redirect_cb(struct evhttp_request *request, void *arg)
{
	struct evkeyvalq *headers = evhttp_request_get_output_headers(request);
	evhttp_add_header(headers, "Location", arg);
	struct evbuffer *buf = evbuffer_new();
	evbuffer_add_printf(buf, "Redirecting to %s", (char *) arg);
	evhttp_send_reply(request, HTTP_MOVETEMP, "Redirect", buf);
	evbuffer_free(buf);
}
