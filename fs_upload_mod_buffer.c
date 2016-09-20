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
#include <aura-httpd/vfs.h>
#include <aura-httpd/nodefs.h>
#include <aura-httpd/uploadfs.h>

struct available_buffer {
	struct aura_buffer *buf;
	bool in_use;
	uint64_t lastused;
};

struct bufmod_data {
	struct aura_node *node;
	int num_buffers;
	struct available_buffer *bufs;
};

static int buffer_init(struct upfs_data *fsd)
{
	struct json_object *props = fsd->mpoint->props;
	struct bufmod_data *fdata = calloc(sizeof(*fdata), 1);

	if (!fdata)
		return -ENOMEM;
	fsd->mod_data = fdata;

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

	return 0;

err_free_fdata:
	free(fdata);
	return -EIO;
}

static void buffer_deinit(struct upfs_data *fsd)
{
	printf("[UPLOAD_MOD_DEBUG] DEINIT!\n");
}

static void buffer_handle_rq_headers(struct upfs_data *fsd)
{
	printf("[UPLOAD_MOD_DEBUG] Incoming upload request!\n");
}

static void buffer_handle_form_header(struct upfs_data *fsd, char *key, char *value)
{
	printf("[UPLOAD_MOD_DEBUG] form header |  %s: %s\n", key, value);
}

static void buffer_handle_data(struct upfs_data *fsd, struct evbuffer_iovec *vec, int length)
{
	printf("[UPLOAD_MOD_DEBUG] Writing %d bytes of data to file\n", length);
	if (0 != dump_iovec_to_file("/tmp/data.bin", vec, length))
		uploadfs_upload_send_error(fsd, NULL);
}

static void buffer_send_result(struct upfs_data *fsd)
{
	printf("[UPLOAD_MOD_DEBUG] Sending result!\n");
	ahttpd_reply_accepted(fsd->request, "/");
	return;
}

static struct uploadfs_module debugmod = {
	.name			= "buffer",
	.init			= buffer_init,
	.deinit			= buffer_deinit,
	.inbound_request_hook	= buffer_handle_rq_headers,
	.handle_form_header	= buffer_handle_form_header,
	.handle_data		= buffer_handle_data,
	.send_upload_reply	= buffer_send_result,
};

AHTTPD_UPLOADFS_MODULE(debugmod);
