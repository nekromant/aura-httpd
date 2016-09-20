#ifndef VFS_H
#define VFS_H

#define AHTTPD_FS(s)                                               \
	static void __attribute__((constructor(101))) do_reg_ ## s(void) { \
		ahttpd_filesystem_register(&s);                    \
	}

struct ahttpd_mountpoint;
struct ahttpd_fs {
	char *			name;
	int			usage;
	int			fsdatalen;
	int			(*mount)(struct ahttpd_mountpoint *mpoint);
	void			(*unmount)(struct ahttpd_mountpoint *mpoint);
	void			(*route)(struct evhttp_request *r, struct ahttpd_mountpoint *mpoint);
	struct list_head	qentry;
};

struct ahttpd_mountpoint {
	const char *		mountpoint;
	json_object *		props;
	struct ahttpd_server *	server;
	const struct ahttpd_fs *fs;
	void *			fsdata;
	struct list_head	qentry;
};

void ahttpd_filesystem_register(struct ahttpd_fs *fs);
int ahttpd_mount(struct ahttpd_server *server, json_object *opts);
void ahttpd_unmount(struct ahttpd_mountpoint *mp);
struct ahttpd_mountpoint *ahttpd_mountpoint_lookup(struct ahttpd_server* server, const char *mountpoint);


#endif /* end of include guard: VFS_H */
