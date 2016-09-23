#ifndef UPLOADFS_H
#define UPLOADFS_H

struct uploaded_file
{
	const char *boundary;
};

struct upfs_data;
struct uploadfs_module
{
	const char *name;
	int  (*init)(struct upfs_data *fsd);
	void (*deinit)(struct upfs_data *fsd);
	void (*inbound_request_hook)(struct upfs_data *fsd);
	void (*handle_form_header)(struct upfs_data *fsd, char *key, char *data);
	void (*handle_data)(struct upfs_data *fsd, struct evbuffer_iovec *vec, int n);
	void (*finalize)(struct upfs_data *fsd, int ok);
	struct list_head qentry;
};

struct upfs_data {
	struct iovec *iovec;
	int num_iovec;
	struct uploaded_file *files;
	struct uploadfs_module *mod;
	struct evhttp_request *request;
	int upload_error;
	void *mod_data;
	struct ahttpd_mountpoint *mpoint;
};


void uploadfs_register_module(struct uploadfs_module* mod);
void uploadfs_upload_send_error(struct upfs_data *fsd, struct json_object *reply);
char *uploadfs_get_content_disposition_filename(char *cds_string);
char *uploadfs_get_content_disposition_name(char *cds_string);

int dump_iovec(FILE *fd, struct evbuffer_iovec *vec, int length);
int dump_iovec_to_file(const char *path, struct evbuffer_iovec *vec, int length);

#define AHTTPD_UPLOADFS_MODULE(s)						   \
        static void __attribute__((constructor (101))) do_reg_##s(void) { \
                uploadfs_register_module(&s);			   \
        }

#endif /* end of include guard: UPLOADFS_H */
