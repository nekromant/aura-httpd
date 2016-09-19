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
#include <aura-httpd/utils.h>
#include <aura-httpd/uploadfs.h>


static int buffer_init(struct upfs_data *fsd)
{
	printf("[UPLOAD_MOD_DEBUG] INIT!\n");
	return 0;
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
	.name = "buffer",
	.init = buffer_init,
	.deinit = buffer_deinit,
	.inbound_request_hook = buffer_handle_rq_headers,
    .handle_form_header = buffer_handle_form_header,
	.handle_data = buffer_handle_data,
	.send_upload_reply = buffer_send_result,
};

AHTTPD_UPLOADFS_MODULE(debugmod);
