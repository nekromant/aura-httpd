#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <fcntl.h>
#include <http_parser.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/server.h>
#include <aura-httpd/vfs.h>
#include <aura-httpd/nodefs.h>
#include <aura-httpd/uploadfs.h>
#include <aura-httpd/json.h>

struct available_buffer {
	struct aura_buffer *	buf;
	struct json_object *	info;
	bool			in_use;
	bool 			registered;
	uint64_t		lastused;
	struct list_head	qentry;
	long id;
	struct upfs_data *owner;
};

struct bufmod_data {
	struct aura_node *		node;
	size_t				max_size;
	size_t				current_size;
	struct json_object      *all_objects;
	struct available_buffer *	current_buf;
	struct json_object *		current_info;
	struct list_head		buffers;
	long current_id;
};

static const char dload_fmt[] = "/download/%lu";
static const char drop_fmt[] = "/drop/%lu";

static void rq_download_buffer(struct evhttp_request *request, void *arg);
static void rq_drop_buffer(struct evhttp_request *request, void *arg);

static void buf_avail_register(struct available_buffer *buf)
{
	char tmp[48];
	sprintf(tmp, "/download/%lu", buf->id);
	ahttpd_add_path(buf->owner->mpoint, tmp, rq_download_buffer, buf);
	sprintf(tmp, "/drop/%lu", buf->id);
	ahttpd_add_path(buf->owner->mpoint, tmp, rq_drop_buffer, buf);
	buf->registered = true;
}

static void buf_avail_deregister(struct available_buffer *buf)
{
	char tmp[48];
	sprintf(tmp, dload_fmt, buf->id);
	ahttpd_del_path(buf->owner->mpoint, tmp);
	sprintf(tmp, drop_fmt, buf->id);
	ahttpd_del_path(buf->owner->mpoint, tmp);
	buf->registered = false;
}

static void buf_avail_drop(struct available_buffer *buf)
{
	if (!buf)
		return;
	if (buf->registered)
		buf_avail_deregister(buf);

	if (buf->buf)
		aura_buffer_release(buf->buf);
	if (buf->info)
		json_object_put(buf->info);
	if (!list_empty(&buf->qentry))
		list_del(&buf->qentry);
	free(buf);
}


static void rq_list_buffers(struct evhttp_request *request, void *arg)
{
	struct json_object *ret;
	struct upfs_data *fsd = arg;
	struct bufmod_data *bdata = fsd->mod_data;
	ret = json_object_new_array();
	if (!ret)
		goto err;

	struct available_buffer *pos;
	list_for_each_entry(pos, &bdata->buffers, qentry) {
		json_object_array_add(ret, pos->info);
		json_object_get(pos->info);
	}
	ahttpd_reply_with_json(request, ret);
	json_object_put(ret);
	return;
err:
	evhttp_send_error(request, 500, "Failed to create json");
}

static void rq_download_buffer(struct evhttp_request *request, void *arg)
{
	struct available_buffer *buf = arg;
	struct evkeyvalq *headers = evhttp_request_get_output_headers(request);
	struct evbuffer *buffer = evbuffer_new();

	slog(4, SLOG_DEBUG, "Downloading buf #%lu", buf->id);
	evhttp_add_header(headers, "Content-Type", "application/octet-stream");

	if (!buffer) {
		evhttp_send_error(request, 500, "Out of resources");
		return;
	}
	/* TODO: Keep track of valid data length in aura buffer */
	aura_buffer_rewind(buf->buf);
	const char *data = aura_buffer_get_bin(buf->buf, aura_buffer_length(buf->buf));
	evbuffer_add(buffer, data, buf->buf->size);
	evhttp_send_reply(request, HTTP_OK, "OK", buffer);
	evbuffer_free(buffer);
}

static void rq_drop_buffer(struct evhttp_request *request, void *arg)
{
	struct available_buffer *buf = arg;
	struct upfs_data *fsd = buf->owner;

	buf_avail_drop(buf);
	ahttpd_reply_accepted(request, fsd->mpoint->mountpoint);
}

static int buffer_init(struct upfs_data *fsd)
{
	struct json_object *props = fsd->mpoint->props;
	struct bufmod_data *fdata = calloc(sizeof(*fdata), 1);

	if (!fdata)
		return -ENOMEM;
	fsd->mod_data = fdata;
	INIT_LIST_HEAD(&fdata->buffers);
	const char *owner = json_array_find_string(props, "owner");
	if (!owner) {
		slog(0, SLOG_ERROR, "Upload mode 'buffer' requires 'owner' parameter to be set");
		goto err_free_fdata;
	}

	struct ahttpd_mountpoint *mp;
	mp = ahttpd_mountpoint_lookup(fsd->mpoint->server, owner);
	if (!mp) {
		slog(0, SLOG_ERROR, "Failed to look up mountpoint: %s", owner);
		goto err_free_fdata;
	}
	if (strcmp(mp->fs->name, "node") != 0) {
		slog(0, SLOG_ERROR,
		     "Upload mode buffer requires owner mountpoint of type 'node', have '%s'",
		     mp->fs->name);
		goto err_free_fdata;
	}

	struct nodefs_data *npd = mp->fsdata;
	fdata->node = npd->node;
	fdata->max_size = 8 * 1024 * 1024;
	slog(1, SLOG_INFO,
	     "aura buffer uploader @ %s. Max size %ld",
	     fsd->mpoint->mountpoint, fdata->max_size);

	ahttpd_add_path(fsd->mpoint, "/", rq_list_buffers, fsd);
	return 0;

err_free_fdata:
	free(fdata);
	return -EIO;
}

static void buffer_deinit(struct upfs_data *fsd)
{
	struct bufmod_data *bdata = fsd->mod_data;
	free(fsd->mod_data);
	/* TODO: Free all associated buffers */
}

static void buffer_handle_rq_headers(struct upfs_data *fsd)
{
	struct bufmod_data *bdata = fsd->mod_data;

	if (bdata->current_info) {
		slog(0, SLOG_WARN, "Stale info object, this should't happen");
		json_object_put(bdata->current_info);
		bdata->current_info = NULL;
	}

	if (bdata->current_buf) {
		slog(4, SLOG_WARN, "WARN: Dropping a stale buffer, that shouldn't happen");
		buf_avail_drop(bdata->current_buf);
		bdata->current_buf = NULL;
	}

	bdata->current_info = json_object_new_array();
	if (!bdata->current_info)
		uploadfs_upload_send_error(fsd, NULL);
}

static void buffer_handle_form_header(struct upfs_data *fsd, char *key, char *value)
{
	struct bufmod_data *bdata = fsd->mod_data;

	struct available_buffer *buf = bdata->current_buf;

	if (!buf) {
		buf = calloc(sizeof(*buf), 1);
		if (!buf)
			goto err_mem;
		bdata->current_buf = buf;
		INIT_LIST_HEAD(&buf->qentry);
		buf->info = json_object_new_object();
		if (!buf->info)
			goto err_mem;
		buf->owner = fsd;
	}

	if (strcmp(key, "Content-Disposition") == 0) {
		char *fn;
		struct json_object *name, *filename;
		filename = name = NULL;
		fn = uploadfs_get_content_disposition_filename(value);
		slog(4, SLOG_DEBUG, "filename: %s", fn);
		if (fn) {
			filename = json_object_new_string(fn);
			free(fn);
		};

		fn = uploadfs_get_content_disposition_name(value);
		slog(4, SLOG_DEBUG, "name: %s", fn);
		if (fn) {
			name = json_object_new_string(fn);
			free(fn);
		}
		json_object_object_add(buf->info, "filename", filename);
		json_object_object_add(buf->info, "name", name);
	}
	return;
err_mem:
	buf_avail_drop(buf);
	uploadfs_upload_send_error(fsd, NULL);
	return;
}


static void buffer_handle_data(struct upfs_data *fsd, struct evbuffer_iovec *vec, int length)
{
	struct bufmod_data *bdata = fsd->mod_data;
	struct available_buffer *buf = bdata->current_buf;
	if (bdata->current_size + length > bdata->max_size)
		goto err_mem;

	buf->buf = aura_buffer_from_eviovec(bdata->node, vec, length);
	buf->id = bdata->current_id++;
	if (!buf->buf)
		goto err_mem;

	struct json_object *ido = json_object_new_int64(buf->id);
	if (!ido)
		goto err_mem;

	json_object_object_add(buf->info, "id", ido);
	list_add_tail(&buf->qentry, &bdata->buffers);
	json_object_array_add(bdata->current_info, buf->info);
	json_object_get(buf->info);

	buf_avail_register(buf);
	bdata->current_size += length;
	bdata->current_buf = NULL;
	return;
err_mem:
	buf_avail_drop(buf);
	uploadfs_upload_send_error(fsd, NULL);
}

static void buffer_send_result(struct upfs_data *fsd, int ok)
{
	struct bufmod_data *bdata = fsd->mod_data;

	if (ok) {
		ahttpd_reply_with_json(fsd->request, bdata->current_info);
		json_object_put(bdata->current_info);
		return;
	} else {
		if (bdata->current_buf) {
			if (bdata->current_buf->buf)
				aura_buffer_release(bdata->current_buf->buf);
			free(bdata->current_buf);
		}
	}
}

static struct uploadfs_module debugmod = {
	.name			= "buffer",
	.init			= buffer_init,
	.deinit			= buffer_deinit,
	.inbound_request_hook	= buffer_handle_rq_headers,
	.handle_form_header	= buffer_handle_form_header,
	.handle_data		= buffer_handle_data,
	.finalize		= buffer_send_result,
};

AHTTPD_UPLOADFS_MODULE(debugmod);
