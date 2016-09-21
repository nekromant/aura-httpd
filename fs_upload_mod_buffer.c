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
	char *			original_filename;
	bool			in_use;
	uint64_t		lastused;
	struct list_head	qentry;
};

struct bufmod_data {
	struct aura_node *		node;
	size_t				max_size;
	size_t				current_size;
	struct available_buffer *	current_buf;
	struct list_head		buffers;
};

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

	return 0;

err_free_fdata:
	free(fdata);
	return -EIO;
}

static void buffer_deinit(struct upfs_data *fsd)
{
	free(fsd->mod_data);
	/* TODO: Free all associated buffers */
}

static void buffer_handle_rq_headers(struct upfs_data *fsd)
{
	struct bufmod_data *bdata = fsd->mod_data;
	struct available_buffer *buf = calloc(sizeof(*buf), 1);
	if (!buf)
		uploadfs_upload_send_error(fsd, NULL);
	bdata->current_buf = buf;
}

static void buffer_handle_form_header(struct upfs_data *fsd, char *key, char *value)
{
	struct bufmod_data *bdata = fsd->mod_data;
	if (strcmp(key, "Content-Disposition")==0) {
		char *fn = uploadfs_get_content_disposition_filename(value);
		bdata->current_buf->original_filename = fn;
	}
}


/* TODO: Move these to aura */
void aura_buffer_put_eviovec(struct aura_buffer *	buf,
			     struct evbuffer_iovec *	vec,
			     size_t			length)
{
	int i = 0;

	while (length) {
		size_t towrite = min_t(size_t, length, vec[i].iov_len);
		aura_buffer_put_bin(buf, vec[i].iov_base, towrite);
		i++;
		length -= towrite;
	}
}

struct aura_buffer *aura_buffer_from_eviovec(struct aura_node *		node,
					     struct evbuffer_iovec *	vec,
					     size_t			length)
{
	struct aura_buffer *buf = aura_buffer_request(node, length);

	if (!buf)
		return NULL;
	aura_buffer_put_eviovec(buf, vec, length);
	return buf;
}

static void buffer_handle_data(struct upfs_data *fsd, struct evbuffer_iovec *vec, int length)
{
	printf("[UPLOAD_MOD_DEBUG] Writing %d bytes of data to file\n", length);
	if (0 != dump_iovec_to_file("/tmp/data.bin", vec, length))
		uploadfs_upload_send_error(fsd, NULL);
}

static void buffer_send_result(struct upfs_data *fsd, int ok)
{
	struct bufmod_data *bdata = fsd->mod_data;

	if (ok) {
		ahttpd_reply_accepted(fsd->request, "/");
		list_add_tail(&bdata->current_buf->qentry, &bdata->buffers);
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
