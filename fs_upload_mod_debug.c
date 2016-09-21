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
#include <aura-httpd/uploadfs.h>


static int debug_init(struct upfs_data *fsd)
{
	printf("[UPLOAD_MOD_DEBUG] INIT!\n");
	return 0;
}

static void debug_deinit(struct upfs_data *fsd)
{
	printf("[UPLOAD_MOD_DEBUG] DEINIT!\n");
}

static void debug_handle_rq_headers(struct upfs_data *fsd)
{
	printf("[UPLOAD_MOD_DEBUG] Incoming upload request!\n");
}

static void debug_handle_form_header(struct upfs_data *fsd, char *key, char *value)
{
	printf("[UPLOAD_MOD_DEBUG] form header |  %s: %s\n", key, value);
}

static void debug_handle_data(struct upfs_data *fsd, struct evbuffer_iovec *vec, int length)
{
	printf("[UPLOAD_MOD_DEBUG] Writing %d bytes of data to file\n", length);
	if (0 != dump_iovec_to_file("/tmp/data.bin", vec, length))
		uploadfs_upload_send_error(fsd, NULL);
}

static void debug_send_result(struct upfs_data *fsd, int ok)
{
	printf("[UPLOAD_MOD_DEBUG] Upload %s\n", ok ? "succeded" : "failed");
	ahttpd_reply_accepted(fsd->request, "/");
	return;
}

static struct uploadfs_module debugmod = {
	.name = "debug",
	.init = debug_init,
	.deinit = debug_deinit,
	.inbound_request_hook = debug_handle_rq_headers,
    .handle_form_header = debug_handle_form_header,
	.handle_data = debug_handle_data,
	.finalize = debug_send_result,
};

AHTTPD_UPLOADFS_MODULE(debugmod);
