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

static  LIST_HEAD(mod_registry);

void uploadfs_register_module(struct uploadfs_module* mod)
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

//---------------------------42857694278218035463629225
//-----------------------------42857694278218035463629225
//-----------------------------42857694278218035463629225
//-----------------------------42857694278218035463629225--

//---------------------------12488422001923715670204120959
//-----------------------------6621722563964962921996144706
//-----------------------------6621722563964962921996144706--

static void upload_error(struct evhttp_request *request) {
		evhttp_send_error(request, 406, "Not acceptable");
}

const unsigned char *request_check_headers(struct upfs_data *fsd, struct evhttp_request *req)
{
	struct evkeyvalq *headers = evhttp_request_get_input_headers(req);
	if (!headers) {
		return NULL;
	}
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
	};

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

static int multipart_handle_next_file(struct upfs_data* fsd, struct evbuffer *in_evb, const char *boundary)
{

	int ret = -EBADMSG;
	int count;
	char *boundary_search = malloc(strlen(boundary)+4);
	if (!boundary_search)
		return -ENOMEM;

	sprintf(boundary_search, "--%s", boundary);

	struct evbuffer_ptr start, next;
	start = evbuffer_search(in_evb, boundary_search, strlen((char *) boundary_search), NULL);
	if (start.pos == -1)
		goto bailout;

	evbuffer_ptr_set(in_evb, &start, strlen(boundary_search), EVBUFFER_PTR_ADD);

	next = evbuffer_search(in_evb, "\r\n\r\n", 4, NULL);
	if (next.pos == -1)
		goto bailout;

	ssize_t length = next.pos - start.pos;
	slog(4, SLOG_DEBUG, "File headers from %d to %d len %lu", start.pos, next.pos, length);

	/* sanity check */
	if (next.pos < start.pos)
		goto bailout;

	

	count = evbuffer_peek_realloc(in_evb, length, &start, &fsd->iovec, &fsd->num_iovec);
	if (count <= 0)
		goto bailout;

	if (0 != fsd->mod->handle_headers(fsd, fsd->iovec, length))
		goto bailout;

	start = next;
	evbuffer_ptr_set(in_evb, &start, 4, EVBUFFER_PTR_ADD);

	next  = evbuffer_search(in_evb, boundary_search, strlen((char *) boundary_search), &start);
	if (next.pos == -1)
		goto bailout;

	if (next.pos < start.pos)
		goto bailout;

	slog(4, SLOG_DEBUG, "File data from %d to %d", start.pos, next.pos);

	/* There's an additional \r\n just before the terminating boundary string */
	length = next.pos - start.pos - 2;
	count = evbuffer_peek_realloc(in_evb, length, &start, &fsd->iovec, &fsd->num_iovec);
	if (count <= 0)
		goto bailout;

	if (0 != fsd->mod->handle_data(fsd, fsd->iovec, length))
		goto bailout;

	/* Now, discard all the data that we've processed */
	evbuffer_drain(in_evb, next.pos);
	dump_evbuffer(in_evb, evbuffer_get_length(in_evb));
	/* And see if trailing boundary indicates more files */
	sprintf(boundary_search, "%s--", boundary);
	char *tmp = (char *) evbuffer_pullup(in_evb, strlen(boundary_search));
	if (strncmp(tmp, boundary_search, strlen(boundary_search))==0)
		ret = 0; /* This looks like the last file */
	else
		ret = 1; /* We might have more to handle */


//	char tmp[16];
//	evbuffer_copyout(in_evb, tmp, 16);
//	tmp[15]=0x0;
//	slog(0, SLOG_LIVE, "====> %s", tmp);

	/* Is this the last file? If yes, we'll have extra -- at the end of boundary */

	slog(0, SLOG_DEBUG, "File @ %d <--> %d; Have more: %d \n", start.pos, next.pos, ret);
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

	size_t len = evbuffer_get_length(evhttp_request_get_input_buffer(request));
	struct evbuffer *in_evb = evhttp_request_get_input_buffer(request);

	const unsigned char *boundary = request_check_headers(fsd, request);
	if (!boundary) {
		upload_error(request);
		return;
	}

	slog(0, SLOG_DEBUG, "Uploading %d bytes, boundary %s", len, boundary);

	int ret;
	/* Handle files to upload */
	do {
		ret = multipart_handle_next_file(fsd, in_evb, (char *)boundary);
	} while (ret == 1);

	if (ret < 0) {
		upload_error(request);
		return;
	}



/*
	evbuffer_find(in_evb, boundary, strlen(boundary));



	memset(&fsd->settings, 0x0, sizeof(fsd->settings));
	fsd->settings.on_body = handle_file_cb;

	http_parser_init(&fsd->parse, HTTP_BOTH);
	feed_request_start(request, &fsd->parse, &fsd->settings);

	if (0 != feed_request_headers(request, &fsd->parse, &fsd->settings))
		slog(2, SLOG_WARN, "Parsing request headers failed");

	while (len) {
		int tocopy  = evbuffer_get_contiguous_space(in_evb);
		if (tocopy <= 0)
			break;
		tocopy = min_t(int, tocopy, len);
		unsigned char *data  = evbuffer_pullup(in_evb, tocopy);
		int nparsed = http_parser_execute(&fsd->parse, &fsd->settings,
			(char *) data, tocopy);
		if (nparsed < tocopy) {
			break;
		}
		evbuffer_drain(in_evb, tocopy);
		len -= tocopy;
	}

	http_parser_execute(&fsd->parse, &fsd->settings, NULL, 0);
	*/
	ahttpd_reply_accepted(request, "/blah");
	return;
}


static int up_mount(struct ahttpd_mountpoint *mpoint)
{
	struct upfs_data *fsd = mpoint->fsdata;

	const char *modname = "debug";
	if (0 != uploadfs_module_select(fsd, modname)){
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
;
