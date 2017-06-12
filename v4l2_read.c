#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <aura-httpd/server.h>
#include <aura-httpd/json.h>
#include <aura-httpd/vfs.h>
#include <aura-httpd/v4l2fs.h>
#include <linux/videodev2.h>



#define CLEAR(x) memset(&(x), 0, sizeof(x))

static int xioctl(int fh, int request, void *arg)
{
	int r;

	do
		r = ioctl(fh, request, arg);
	while (-1 == r && EINTR == errno);

	return r;
}

static void free_frames(struct v4l2fs_buffer *frames)
{
	int i = 0;

	while (frames[i].start) {
		free(frames[i].start);
		i++;
	}
	free(frames);
}

static struct v4l2fs_buffer *allocate_frames(int count, size_t frame_size)
{
	struct v4l2fs_buffer *frames;

	frames = calloc(count + 1, sizeof(*frames));

	if (!frames)
		return NULL;
	int i;
	for (i = 0; i < count; i++) {
		frames[i].start = calloc(1, frame_size);
		if (!frames[i].start) {
			free_frames(frames);
			return NULL;
		}
		frames[i].length = frame_size;
	}
	return frames;
}

static int open_device(const char *dev_name)
{
	struct stat st;
	int fd;

	if (-1 == stat(dev_name, &st)) {
		slog(0, SLOG_ERROR, "Cannot identify '%s': %d, %s",
		     dev_name, errno, strerror(errno));
		return -1;
	}

	if (!S_ISCHR(st.st_mode)) {
		slog(0, SLOG_ERROR, "%s is no device", dev_name);
		return -1;
	}

	fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

	if (-1 == fd) {
		slog(0, SLOG_ERROR, "Cannot open '%s': %d, %s",
		     dev_name, errno, strerror(errno));
		return -1;
	}
	return fd;
}

static void device_common_cleanup(struct v4l2fs_grabber_instance *data)
{
	close(data->fd);
	data->fd = -1;
	if (data->buffers) {
		if (strcmp(data->grb->name, "mmap") != 0)
			free_frames(data->buffers);
		data->buffers = NULL;
	}
}


static int device_get_input(struct v4l2fs_grabber_instance *data, char **inputname)
{
	struct v4l2_input input;
	int index;

	if (-1 == ioctl(data->fd, VIDIOC_G_INPUT, &index)) {
		slog(0, SLOG_ERROR, "VIDIOC_G_INPUT failed: %s", strerror(errno));
		return -1;
	}

	memset(&input, 0, sizeof(input));
	input.index = index;

	if (-1 == ioctl(data->fd, VIDIOC_ENUMINPUT, &input)) {
		slog(0, SLOG_ERROR, "VIDIOC_ENUMINPUT failed: %s", strerror(errno));
		return -1;
	}

	if (inputname)
		*inputname = strdup((char *)input.name);

	return index;
}

static int device_set_input(struct v4l2fs_grabber_instance *data, int index)
{
	if (-1 == ioctl(data->fd, VIDIOC_S_INPUT, &index)) {
		slog(0, SLOG_ERROR, "VIDIOC_S_INPUT failed: %s", strerror(errno));
		return -1;
	}
	return 0;
}


static int device_common_init(struct v4l2fs_grabber_instance *data, struct v4l2_capability *cap)
{
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	unsigned int min;


	if (data->parent) {
		slog(1, SLOG_INFO, "Virtual device, skipping initialization and overriding everything with parent params");
		data->fd = data->parent->fd;
		data->width = data->parent->width;
		data->height = data->parent->height;
		data->frame_count = data->parent->frame_count;
		data->buffers = data->parent->buffers;
		data->frame_size = data->parent->frame_size;
		return 1;
	}

	data->fd = open_device(data->device);
	if (data->fd < 0)
		return -1;

	if (-1 == xioctl(data->fd, VIDIOC_QUERYCAP, cap)) {
		if (EINVAL == errno) {
			slog(0, SLOG_ERROR, "%s is NOT a V4L2 device",
			     data->device);
			goto bailout;
		} else {
			slog(0, SLOG_ERROR, "VIDIOC_QUERYCAP failed");
			goto bailout;
		}
	}

	if (!(cap->capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		slog(0, SLOG_ERROR, "%s is no video capture device\n",
		     data->device);
		goto bailout;
	}

	slog(4, SLOG_DEBUG, "v4l2_read: Using device %s", data->device);



	CLEAR(cropcap);

	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (0 == xioctl(data->fd, VIDIOC_CROPCAP, &cropcap)) {
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect;     /* reset to default */

		if (-1 == xioctl(data->fd, VIDIOC_S_CROP, &crop)) {
			switch (errno) {
			case EINVAL:
				/* Cropping not supported. */
				break;
			default:
				/* Errors ignored. */
				break;
			}
		}
	} else {
		/* Errors ignored. */
	}


	CLEAR(fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	fmt.fmt.pix.width = data->width;
	fmt.fmt.pix.height = data->height;
	/* TODO: ... */
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	fmt.fmt.pix.field = V4L2_FIELD_ANY;

	if (-1 == xioctl(data->fd, VIDIOC_S_FMT, &fmt)) {
		slog(0, SLOG_ERROR, "VIDIOC_S_FMT failed");
		goto bailout;
	}

#if 0
	/* Preserve original settings as set by v4l2-ctl for example */
	if (-1 == xioctl(data->fd, VIDIOC_G_FMT, &fmt))
		errno_exit("VIDIOC_G_FMT");
}
#endif

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	data->frame_size = fmt.fmt.pix.sizeimage;
	return 0;

bailout:
	close(data->fd);
	device_common_cleanup(data);
	return -1;
}




static int device_read_init(struct v4l2fs_grabber_instance *data)
{
	struct v4l2_capability cap;
	int ret = device_common_init(data, &cap);

	if (ret < 0)
		return ret;
	if (ret > 0)
		return 0; /* Virtual device */

	if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
		slog(0, SLOG_ERROR, "%s does not support read i/o",
		     data->device);
		goto bailout;
	}

	/* Allocate space for one frame, we won't need any more*/
	data->buffers = allocate_frames(1, data->frame_size);
	if (!data->buffers) {
		slog(0, SLOG_ERROR, "Out of memory\n");
		goto bailout;
	}

	return 0;

bailout:
	device_common_cleanup(data);
	return -1;
}

static int device_mmap_init(struct v4l2fs_grabber_instance *data)
{
	struct v4l2_capability cap;
	int ret = device_common_init(data, &cap);

	if (ret < 0)
		return ret;
	if (ret > 0)
		return 0; /* Virtual device */

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		slog(0, SLOG_ERROR, "%s does not support streaming (mmap/userptr) i/o",
		     data->device);
		goto bailout;
	}

	struct v4l2_requestbuffers req;
	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(data->fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			slog(0, SLOG_ERROR, "%s does not support "
			     "memory mapping\n", data->device);
			goto bailout;
		} else {
			slog(0, SLOG_ERROR, "VIDIOC_REQBUFS failed!");
			goto bailout;
		}
	}

	if (req.count < 2) {
		slog(0, SLOG_ERROR, "Insufficient buffer memory on %s\n",
		     data->device);
		goto bailout;
	}

	data->buffers = calloc(req.count, sizeof(*data->buffers));
	if (!data->buffers) {
		slog(0, SLOG_ERROR, "Out of memory\n");
		goto bailout;
	}

	for (data->frame_count = 0; data->frame_count < req.count; ++data->frame_count) {
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = data->frame_count;

		if (-1 == xioctl(data->fd, VIDIOC_QUERYBUF, &buf)) {
			slog(0, SLOG_ERROR, "VIDIOC_QUERYBUF failed!");
			goto bailout;
		}

		data->buffers[data->frame_count].length = buf.length;
		data->buffers[data->frame_count].start =
			mmap(NULL /* start anywhere */,
			     buf.length,
			     PROT_READ | PROT_WRITE /* required */,
			     MAP_SHARED /* recommended */,
			     data->fd, buf.m.offset);

		if (MAP_FAILED == data->buffers[data->frame_count].start) {
			slog(0, SLOG_ERROR, "mmap failed for buffer %d!", data->frame_count);
			goto bailout;
		}
	}
	return 0;

bailout:
	device_common_cleanup(data);
	return -1;
}


static int device_userptr_init(struct v4l2fs_grabber_instance *data)
{
	struct v4l2_capability cap;
	int ret = device_common_init(data, &cap);

	if (ret < 0)
		return ret;
	if (ret > 0)
		return 0;         /* Virtual device */

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		slog(0, SLOG_ERROR, "%s does not support streaming (mmap/userptr) i/o",
		     data->device);
		goto bailout;
	}

	struct v4l2_requestbuffers req;
	CLEAR(req);

	req.count = 4;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_USERPTR;

	if (-1 == xioctl(data->fd, VIDIOC_REQBUFS, &req)) {
		if (EINVAL == errno) {
			slog(0, SLOG_ERROR, "%s does not support "
			     "user pointer i/o", data->device);
			goto bailout;
		} else {
			slog(0, SLOG_ERROR, "VIDIOC_REQBUFS failed");
			goto bailout;
		}
	}

	data->buffers = allocate_frames(req.count, data->frame_size);
	if (!data->buffers) {
		slog(0, SLOG_ERROR, "Out of memory");
		goto bailout;
	}
	return 0;

bailout:
	device_common_cleanup(data);
	return -1;
}


static int  start_read(struct v4l2fs_grabber_instance *data)
{
	return 0;
	/* No-op */
}

static int  start_mmap(struct v4l2fs_grabber_instance *data)
{
	int i;
	enum v4l2_buf_type type;

	for (i = 0; i < data->frame_count; ++i) {
		struct v4l2_buffer buf;

		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == xioctl(data->fd, VIDIOC_QBUF, &buf)) {
			slog(0, SLOG_ERROR, "VIDIOC_QBUF failed for buffer %d!", i);
			return -1;
		}
	}
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(data->fd, VIDIOC_STREAMON, &type)) {
		slog(0, SLOG_ERROR, "VIDIOC_STREAMON failed!");
		return -1;
	}
	return 0;
}

static int start_userptr(struct v4l2fs_grabber_instance *data)
{
	int i;
	enum v4l2_buf_type type;

	for (i = 0; i < data->frame_count; ++i) {
		struct v4l2_buffer buf;

		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_USERPTR;
		buf.index = i;
		buf.m.userptr = (unsigned long)data->buffers[i].start;
		buf.length = data->buffers[i].length;

		if (-1 == xioctl(data->fd, VIDIOC_QBUF, &buf)) {
			slog(0, SLOG_ERROR, "VIDIOC_QBUF failed for buffer %d!", i);
			return -1;
		}
	}

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(data->fd, VIDIOC_STREAMON, &type)) {
		slog(0, SLOG_ERROR, "VIDIOC_STREAMON failed!");
		return -1;
	}
	return 0;
}

static int wait_for_frame(struct v4l2fs_grabber_instance *data)
{
	fd_set fds;
	struct timeval tv;
	int r;

	FD_ZERO(&fds);
	FD_SET(data->fd, &fds);

	/* Timeout. */
	tv.tv_sec = 2;
	tv.tv_usec = 0;

	r = select(data->fd + 1, &fds, NULL, NULL, &tv);

	if (-1 == r) {
		slog(0, SLOG_ERROR, "select() returned error: %s", strerror(errno));
		return -EAGAIN;
	}

	if (0 == r) {
		slog(0, SLOG_ERROR, "select() timeout while waiting for frame data");
		return -ETIMEDOUT;
	}

	return 0; /* Frame ready'n'waiting ! */
}

static int read_frame_read(struct v4l2fs_grabber_instance *data)
{
	if (-1 == read(data->fd, data->buffers[0].start, data->buffers[0].length)) {
		slog(0, SLOG_ERROR, "read() failed: %s", strerror(errno));
		return -1;
	}

	/* Call the ready handler here */
	return 0;
}

static int read_frame_mmap(struct v4l2fs_grabber_instance *data)
{
	struct v4l2_buffer buf;

	CLEAR(buf);

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(data->fd, VIDIOC_DQBUF, &buf)) {
		slog(0, SLOG_ERROR, "VIDIOC_DQBUF failed: %s", strerror(errno));
		return -1;
	}

	if (buf.index < data->frame_count) {
		slog(0, SLOG_ERROR, "VIDIOC_DQBUF reported insane index: %d<%d!", buf.index, data->frame_count);
		return -1;
	}

	//process_image(buffers[buf.index].start, buf.bytesused);

	if (-1 == xioctl(data->fd, VIDIOC_QBUF, &buf)) {
		slog(0, SLOG_ERROR, "VIDIOC_QBUF failed: %s!", strerror(errno));
		return -1;
	}
	return 0;
}

static int read_frame_userptr(struct v4l2fs_grabber_instance *data)
{
	struct v4l2_buffer buf;
	unsigned int i = -1;

	CLEAR(buf);

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_USERPTR;

	if (-1 == xioctl(data->fd, VIDIOC_DQBUF, &buf)) {
		slog(0, SLOG_ERROR, "VIDIOC_DQBUF failed: %s", strerror(errno));
		return -1;
	}

	for (i = 0; i < data->frame_count; ++i)
		if (buf.m.userptr == (unsigned long)data->buffers[i].start
		    && buf.length == data->buffers[i].length)
			break;

	if (buf.index < data->frame_count) {
		slog(0, SLOG_ERROR, "VIDIOC_DQBUF returned invalid pointer!");
		return -1;
	}

	//process_image((void *)buf.m.userptr, buf.bytesused);

	if (-1 == xioctl(data->fd, VIDIOC_QBUF, &buf)) {
		slog(0, SLOG_ERROR, "VIDIOC_QBUF failed: %s!", strerror(errno));
		return -1;
	}

	return 0;
}


static struct v4l2fs_grabber grabbers[] = {
	{
		.name = "read",
		.init = device_read_init,
		.start = start_read,
		.wait_for_frame = wait_for_frame,
		.read_frame = read_frame_read,
		.get_input = device_get_input,
		.set_input = device_set_input,
	},
	{
		.name = "mmap",
		.init = device_mmap_init,
		.start = start_mmap,
		.wait_for_frame = wait_for_frame,
		.read_frame = read_frame_mmap,
		.get_input = device_get_input,
		.set_input = device_set_input,
	},
	{
		.name = "userptr",
		.init = device_userptr_init,
		.start = start_userptr,
		.wait_for_frame = wait_for_frame,
		.read_frame = read_frame_userptr,
		.get_input = device_get_input,
		.set_input = device_set_input,
	},
	{ /* Sentinel */ }
};

struct v4l2fs_grabber *v4l2fs_get_grabber_by_name(const char *name)
{
	struct v4l2fs_grabber *pos = grabbers;

	while (pos->name) {
		if (strcmp(pos->name, name) == 0)
			return pos;
		pos++;
	}
	return NULL;
}
