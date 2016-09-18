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

static LIST_HEAD(mod_registry);

void uploadfs_register_module(struct uploadfs_module *mod)
{
	slog(0, SLOG_DEBUG, "Registering uploadfs module: %s", mod->name);
	list_add_tail(&mod->qentry, &mod_registry);
}

static int uploadfs_module_select(struct upfs_data *fsd, const char *name)
{
	struct uploadfs_module *pos;

	list_for_each_entry(pos, &mod_registry, qentry) {
		if (strcmp(name, pos->name) == 0) {
			fsd->mod = pos;
			return 0;
		}
	}
	return -1;
}

static void dump_evbuffer(struct evbuffer *buf, int len)
{
	char *tmp = malloc(len);

	evbuffer_copyout(buf, tmp, len);
	printf("%s", tmp);
	fflush(stdout);
}

void uploadfs_upload_send_error(struct upfs_data *fsd, struct json_object *reply)
{
	/* Stop any further parsing */
	fsd->upload_error = 1;
	if (reply)
		ahttpd_reply_with_json(fsd->request, reply);
	else
		evhttp_send_error(fsd->request, 406, "Not acceptable");
}

#define check_for_fault() { \
		if (fsd->upload_error) \
			goto bailout; \
}

const unsigned char *request_check_headers(struct upfs_data *fsd, struct evhttp_request *req)
{
	struct evkeyvalq *headers = evhttp_request_get_input_headers(req);

	if (!headers)
		return NULL;

	const char *vl = evhttp_find_header(headers, "Content-Length");

	vl = evhttp_find_header(headers, "Content-Type");
	if (!vl) {
		slog(2, SLOG_WARN, "Missing Content-Type Header");
		return NULL;
	}
	char *tmp = strstr(vl, "multipart/form-data");
	if (!tmp) {
		slog(2, SLOG_WARN, "Not a multipart/form-data");
		return NULL;
	}

	char bnd[] = "boundary=";
	const char *boundary = strstr(vl, bnd);
	if (!boundary) {
		slog(2, SLOG_WARN, "Boundary not found in %s", vl);
		return NULL;
	}
	;

	boundary = &boundary[strlen(bnd)];
	return (unsigned char *)boundary;
}

int dump_iovec(FILE *fd, struct evbuffer_iovec *vec, int length)
{
	int i = 0;

	while (length) {
		ssize_t towrite = min_t(ssize_t, length, vec[i].iov_len);
		slog(4, SLOG_DEBUG, "Extent @0x%x len %d towrite %d/%d", vec[i].iov_base, vec[i].iov_len,
		     towrite, length);
		fwrite(vec[i].iov_base, towrite, 1, fd);
		i++;
		length -= towrite;
	}
	return 0;
}

int dump_iovec_to_file(const char *path, struct evbuffer_iovec *vec, int length)
{
	int ret;
	FILE *fd = fopen(path, "w+");

	if (!fd)
		return -EIO;
	ret = dump_iovec(fd, vec, length);
	fclose(fd);
	return ret;
}

int evbuffer_peek_realloc(struct evbuffer *buffer, ssize_t len, struct evbuffer_ptr *start_at, struct iovec **vec_out, int *n_vec)
{
	int ret = evbuffer_peek(buffer, len, start_at, *vec_out, *n_vec);

	slog(4, SLOG_DEBUG, "Peek %lu ret %d", len, ret);
	if (ret > *n_vec) {
		slog(4, SLOG_DEBUG, "Reallocating iovec: %d->%d\n", *n_vec, ret);
		struct iovec *tmp = realloc(*vec_out, sizeof(struct iovec) * ret);
		if (!tmp)
			return -ENOMEM;
		*n_vec = ret;
		*vec_out = tmp;
		return evbuffer_peek_realloc(buffer, len, start_at, vec_out, n_vec);
	}
	return ret;
}


/*
 *      -1 - immediate break, no free(ln);
 *       0 - continue and free;
 *       1 - break and free()
 *
 */
static int handle_file_header_line(struct upfs_data *fsd, char *ln)
{
	char *tmp;

	if (!ln)
		return -1;

	if (strlen(ln) == 0)
		return 1;

	char *key = strtok_r(ln, ":", &tmp);
	char *value = strtok_r(NULL, "\r\n", &tmp);
	while (*value == ' ') value++;

	if (fsd->mod->handle_form_header)
		fsd->mod->handle_form_header(fsd, key, value);
	if (fsd->upload_error)
		return 1;

	return 0;
}

static int multipart_handle_next_file(struct upfs_data *fsd, struct evbuffer *in_evb, const char *boundary)
{
	int ret = -EBADMSG;
	int count;
	char *boundary_search = malloc(strlen(boundary) + 8);

	if (!boundary_search)
		return -ENOMEM;

	sprintf(boundary_search, "--%s", boundary);

	struct evbuffer_ptr start, next;
	start = evbuffer_search(in_evb, boundary_search, strlen((char *)boundary_search), NULL);
	if (start.pos == -1)
		goto bailout;

	evbuffer_drain(in_evb, strlen(boundary_search) + start.pos + 2);
	ssize_t length;
	while (1) {
		size_t nread;
		int ret;

		char *ln = evbuffer_readln(in_evb, &nread, EVBUFFER_EOL_CRLF);

		ret = handle_file_header_line(fsd, ln);
		if (ret > 0)
			free(ln);
		if (ret != 0)
			break;
	}

	if (fsd->upload_error)
		goto bailout;

	next = evbuffer_search(in_evb, boundary_search, strlen((char *)boundary_search), NULL);
	if (next.pos == -1)
		goto bailout;

	if (next.pos < start.pos)
		goto bailout;

	slog(4, SLOG_DEBUG, "File data from %d to %d", start.pos, next.pos);

	/* There's an additional \r\n just before the terminating boundary string */
	length = next.pos - 2;
	count = evbuffer_peek_realloc(in_evb, length, &start, &fsd->iovec, &fsd->num_iovec);
	if (count <= 0)
		goto bailout;

	if (fsd->mod->handle_data)
		fsd->mod->handle_data(fsd, fsd->iovec, length);

	if (fsd->upload_error)
		goto bailout;

	/* Now, discard all the data that we've processed */
	evbuffer_drain(in_evb, next.pos);
	/* And see if trailing boundary indicates more files */
	sprintf(boundary_search, "--%s--", boundary);
	char *tmp = (char *)evbuffer_pullup(in_evb, strlen(boundary_search));

	if (strncmp(tmp, boundary_search, strlen(boundary_search)) == 0)
		ret = 0;        /* This looks like the last file */
	else
		ret = 1;        /* We might have more to handle */

bailout:
	free(boundary_search);
	return ret;
}

static void upload(struct evhttp_request *request, void *arg)
{
	struct ahttpd_mountpoint *mpoint = arg;
	struct upfs_data *fsd = mpoint->fsdata;

	if (!ahttpd_method_allowed(request, EVHTTP_REQ_POST))
		return;

	fsd->request = request;
	/* reset error state */
	fsd->upload_error = 0;

	/* Fire up the hook, if any */
	if (fsd->mod->inbound_request_hook)
		fsd->mod->inbound_request_hook(fsd);

	if (fsd->upload_error)
		return;

	size_t len = evbuffer_get_length(evhttp_request_get_input_buffer(request));
	struct evbuffer *in_evb = evhttp_request_get_input_buffer(request);

	const unsigned char *boundary = request_check_headers(fsd, request);
	if (!boundary) {
		uploadfs_upload_send_error(fsd, NULL);
		return;
	}

	slog(4, SLOG_DEBUG, "Uploading %d bytes, boundary %s", len, boundary);

	int ret;
	do {
		ret = multipart_handle_next_file(fsd, in_evb, (char *) boundary);
		slog(4, SLOG_DEBUG, "Part Handled with result %d", ret);
	} while (ret == 1);

	if (ret < 0) {
		uploadfs_upload_send_error(fsd, NULL);
		return;
	}

	if (fsd->mod->send_upload_reply)
		fsd->mod->send_upload_reply(fsd);

	return;
}

static int up_mount(struct ahttpd_mountpoint *mpoint)
{
	struct upfs_data *fsd = mpoint->fsdata;

	const char *modname = "debug";

	if (0 != uploadfs_module_select(fsd, modname)) {
		slog(0, SLOG_ERROR, "Failed to find uploadfs module: %s", modname);
		return -EIO;
	}

	/* Start with 4 iovec's */
	fsd->iovec = malloc(sizeof(struct iovec) * 4);
	if (!fsd->iovec)
		return -ENOMEM;

	fsd->num_iovec = 4;
	ahttpd_add_path(mpoint, "/aura_buffer", upload, mpoint);
	return 0;
}

static void up_unmount(struct ahttpd_mountpoint *mpoint)
{
	struct upfs_data *fsd = mpoint->fsdata;

	free(fsd->iovec);
	ahttpd_del_path(mpoint, "/aura_buffer");
	return;
}

static struct ahttpd_fs uploadfs =
{
	.name		= "upload",
	.mount		= up_mount,
	.unmount	= up_unmount,
	.fsdatalen	= sizeof(struct upfs_data),
};

AHTTPD_FS(uploadfs);
