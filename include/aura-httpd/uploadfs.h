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
	int (*handle_headers)(struct upfs_data *fsd, struct evbuffer_iovec *vec, int n);
	int (*handle_data)(struct upfs_data *fsd, struct evbuffer_iovec *vec, int n);
	int (*upload_result)(struct upfs_data *fsd, int success);
	struct list_head qentry;
};

struct upfs_data {
	char *blah;
	struct iovec *iovec;
	int num_iovec;
	struct uploaded_file *files;
	struct uploadfs_module *mod;
};



void uploadfs_register_module(struct uploadfs_module* mod);
int dump_iovec(FILE *fd, struct evbuffer_iovec *vec, int length);
int dump_iovec_to_file(const char *path, struct evbuffer_iovec *vec, int length);

#define AHTTPD_UPLOADFS_MODULE(s)						   \
        static void __attribute__((constructor (101))) do_reg_##s(void) { \
                uploadfs_register_module(&s);			   \
        }

#endif /* end of include guard: UPLOADFS_H */
