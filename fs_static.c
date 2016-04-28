#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>


struct staticfs_data {
	char *htdocs;
	const char *index;
	void (*dirindexfunc)(struct evhttp_request *request, const char *dir, const char *rdir);
};

struct dirindex {
	char *name;
	void (*dirindexfunc)(struct evhttp_request *request, const char *dir, const char *rdir);
};


static void index_none(struct evhttp_request *request,
	const char *dir,
	const char *rdir)
{
	evhttp_send_error(request, 403, "GTFO");
}

static void index_json(struct evhttp_request *request,
	const char *dpath,
	const char *rdir)
{
	DIR *dir;
	struct dirent *ent;
	json_object *root = json_object_new_object();

	dir = opendir(dpath);
	if (dir == NULL)
		return; //TODO:


	while ((ent = readdir(dir)) != NULL) {
		json_object *dio     = json_object_new_object();
		json_object *is_dir;
		is_dir = json_object_new_boolean((ent->d_type & DT_DIR) ? 1 : 0);
		json_object_object_add(dio, "is_dir", is_dir);
		json_object_object_add(root, ent->d_name, dio);
	}
	closedir(dir);
	ahttpd_reply_with_json(request, root);
	json_object_put(root);
}

static void index_html(struct evhttp_request *request,
	const char *dpath,
	const char *rdir)
{
	DIR *dir;
	struct dirent *ent;
	struct evbuffer *buffer;

	buffer = evbuffer_new();
	dir = opendir(dpath);
	if (dir == NULL)
		return; //TODO:

	evbuffer_add_printf(buffer, "<h3>Directory listing for %s</h3><hr>", rdir);
	while ((ent = readdir(dir)) != NULL) {
				/* Hide dotfiles */
				if (strncmp(ent->d_name, ".", 1)==0)
					continue;
				evbuffer_add_printf(buffer, "&middot; <a href=\"%s\">%s</a><br>",
				ent->d_name, ent->d_name);
	}
	evbuffer_add_printf(buffer, "<hr>Powered by aura-httpd %s", aura_get_version());
	closedir(dir);
	evhttp_send_reply(request, HTTP_OK, "OK", buffer);
	evbuffer_free(buffer);
}

struct dirindex dirindextypes[] = {
	{ "none", index_none },
	{ "json", index_json },
	{ "html", index_html },
	/* Sentinel */
	{ NULL, 0 }
};


static int serve_file(struct evhttp_request *request, const char *dpath, const char *mimetype)
{
	struct evbuffer *buffer;
	long sz;
	FILE *fd;

	fd = fopen(dpath, "r");
	if (fd == NULL)
		return -1;

	fseek(fd, 0, SEEK_END);
	sz = ftell(fd);
	rewind(fd);
	fclose(fd);

	int ifd = open(dpath, O_RDONLY);
	if (!ifd)
		return -1;

	struct evkeyvalq *headers = evhttp_request_get_output_headers(request);
	evhttp_add_header(headers, "Content-Type", mimetype);
//	evhttp_add_header(headers, "Server", LIBSRVR_SIGNATURE);

	buffer = evbuffer_new();
	evbuffer_add_file(buffer, ifd, 0, sz);
	evhttp_send_reply(request, HTTP_OK, "OK", buffer);
	evbuffer_free(buffer);
	return 0;
}

void router(struct evhttp_request *r, struct ahttpd_mountpoint *mpoint)
{
	struct staticfs_data *fsd = mpoint->fsdata;

	const char *uri = &evhttp_request_get_uri(r)[strlen(mpoint->mountpoint)];
	//TODO: URI sanity-checking to ward off k00lhax0rz
	char *path = NULL;
	char *ipath = NULL;

	if (-1 == asprintf(&path, "%s/%s", fsd->htdocs, uri)) {
		path = NULL;
		evhttp_send_error(r, 500, "Internal server fuckup. That's all I know");
		goto bailout;
	}

	slog(4, SLOG_DEBUG, "Retrieving %s", path);

	struct stat sbuf;
	int ret = stat(path, &sbuf);
	if (ret != 0) {
		evhttp_send_error(r, 404, "We searched hard but never found love");
		return;
	}

	if (S_ISDIR(sbuf.st_mode)) {
		if (fsd->index) {
			if (-1 == asprintf(&ipath, "%s/%s", path, fsd->index)) {
				ipath = NULL;
				evhttp_send_error(r, 500, "Internal server fuckup. That's all I know");
				goto bailout;
			}
			if (0 == serve_file(r, ipath, "text/html; charset=UTF-8"))
				return;
		}
		fsd->dirindexfunc(r, path, evhttp_request_get_uri(r));
	} else {
		const char *mimetype = ahttpd_mime_guess(mpoint->server->mimedb, path);
		serve_file(r, path, mimetype);
	}

bailout:
	if (path)
		free(path);
	if (ipath)
		free(ipath);
}


static int cfs_mount(struct ahttpd_mountpoint *mpoint)
{
	struct staticfs_data *fsd = mpoint->fsdata;

	fsd->htdocs          = strdup(json_find_string(mpoint->props, "dir"));
	fsd->index           = json_find_string(mpoint->props, "index");
	const char *dirindex = json_find_string(mpoint->props, "dirlist");

	if (fsd->htdocs[strlen(fsd->htdocs)-1]=='/')
		fsd->htdocs[strlen(fsd->htdocs)-1]=0x0;

	slog(2, SLOG_INFO, "Serving static files at %s from %s",
		mpoint->mountpoint, fsd->htdocs);

	slog(2, SLOG_INFO, "Assuming %s is the index",
			fsd->index);

	int i=0;
	fsd->dirindexfunc = index_none;
	while (dirindex && (dirindextypes[i].name != NULL)) {
		if (strcmp(dirindextypes[i].name, dirindex)==0) {
			fsd->dirindexfunc = dirindextypes[i].dirindexfunc;
			slog(2, SLOG_INFO, "Using %s directory index", dirindex);
			break;
		}
		i++;
	}
	return 0;
}

static void cfs_unmount(struct ahttpd_mountpoint *mpoint)
{
	struct staticfs_data *fsd = mpoint->fsdata;
	free(fsd->htdocs);
}

static struct ahttpd_fs staticfs =
{
	.name = "static",
	.mount = cfs_mount,
	.unmount = cfs_unmount,
	.route = router,
	.fsdatalen = sizeof(struct staticfs_data),
};

AHTTPD_FS(staticfs);
