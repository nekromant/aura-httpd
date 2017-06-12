#ifndef AURA_HTTPD_V4L2FS
#define AURA_HTTPD_V4L2FS

struct v4l2fs_data;
struct v4l2fs_grabber_instance;

struct v4l2fs_grabber {
    const char *name;
    int  (*init)(struct v4l2fs_grabber_instance*);
    int  (*deinit)(struct v4l2fs_grabber_instance*);

    int  (*start)(struct v4l2fs_grabber_instance*);
    int  (*wait_for_frame)(struct v4l2fs_grabber_instance*);
    int  (*read_frame)(struct v4l2fs_grabber_instance*);
    int  (*stop)(struct v4l2fs_grabber_instance*);
    int  (*get_input)(struct v4l2fs_grabber_instance *data, char **inputname);
    int  (*set_input)(struct v4l2fs_grabber_instance *data, int index);
};

struct v4l2fs_buffer {
        void   *start;
        size_t  length;
};


struct v4l2fs_grabber_instance {
    const char *device;
    int input;
    int width;
    int height;
    struct v4l2fs_grabber *grb;
    int fd;
    struct v4l2fs_buffer *buffers;
    int frame_count;
    size_t frame_size;

    int initialized;
    struct v4l2fs_grabber_instance *parent;
    struct v4l2fs_data *owner;
};

struct v4l2fs_data {
    struct ahttpd_mountpoint *mpoint;
    struct v4l2fs_grabber_instance *instances;
    int num_instances;

    /* TODO: Move these out */
};

struct v4l2fs_grabber *v4l2fs_get_grabber_by_name(const char *name);

#endif /* end of include guard: AURA_HTTPD_V4L2FS */
