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



static int debug_handle_headers(struct upfs_data *fsd, struct evbuffer_iovec *vec, int length)
{
	return dump_iovec_to_file("/tmp/headers.txt", vec, length);
}

static int debug_handle_data(struct upfs_data *fsd, struct evbuffer_iovec *vec, int length)
{
	return dump_iovec_to_file("/tmp/data.bin", vec, length);
}

static int debug_upload_result(struct upfs_data *fsd, int result)
{
	return 0;
}

static struct uploadfs_module debugmod = {
	.name = "debug",
    .handle_headers = debug_handle_headers,
	.handle_data = debug_handle_data,
	.upload_result = debug_upload_result,
};

AHTTPD_UPLOADFS_MODULE(debugmod);
