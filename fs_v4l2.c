#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/server.h>
#include <aura-httpd/vfs.h>
#include <aura-httpd/v4l2fs.h>
#include <aura-httpd/json.h>

static void dispatch_frame(struct evhttp_request *request, void *arg)
{

}

static void cleanup_devices(struct v4l2fs_data *data)
{
	int i;
	for (i=0; i<data->num_instances; i++) {
		if (!data->instances[i].device)
			continue;

		if (data->instances[i].initialized)
			data->instances[i].grb->deinit(&data->instances[i]);
	}
	free(data->instances);
}

static int v4l2_mount(struct ahttpd_mountpoint *mpoint)
{
	struct v4l2fs_data *data = mpoint->fsdata;
	data->mpoint = mpoint;

	struct json_object *obj = json_array_find(mpoint->props, "devices");
	if (!obj) {
		slog(0, SLOG_ERROR, "fs_v4l2: Error parsing config: no 'devices' section");
		return -1;
	}

	enum json_type type = json_object_get_type(obj);
	if (type != json_type_array) {
		slog(0, SLOG_ERROR, "fs_v4l2: Error parsing config: expecting 'devices' section to be an array");
		return -1;
	}

	int arraylen = json_object_array_length(obj);
	int i;

	data->instances = calloc(arraylen, sizeof(*data->instances));
	if (!data->instances) {
		slog(0, SLOG_ERROR, "Out of memory");
		return -1;
	}


	for (i=0; i< arraylen; i++) {
		struct v4l2fs_grabber_instance *inst = &data->instances[i];
		struct json_object *pos;
		pos = json_object_array_get_idx(obj, i);
		type = json_object_get_type(pos);
		if (type != json_type_object) {
			slog(0, SLOG_ERROR, "fs_v4l2: 'devices[%d] is not an object'", i);
			goto epic_fail;
		}

		inst->device = json_array_find_string(pos, "device");
		inst->input  = json_array_find_number(pos, "input");
		inst->height = json_array_find_number(pos, "height");
		inst->width  = json_array_find_number(pos, "width");

		const char *grb_name = json_array_find_string(pos, "iomethod");
		if (grb_name)
			inst->grb = v4l2fs_get_grabber_by_name(grb_name);

		if (!inst->grb) {
			slog(0, SLOG_ERROR, "fs_v4l2: invalid grabbing method: %s",	grb_name);
			goto epic_fail;
		}

		int j;
		for (j=0; j<i; j++) {
			if (strcmp(data->instances[j].device, inst->device)==0) {
				inst->parent = &data->instances[j];
			}
		}

		slog(2, SLOG_INFO, "fs_v4l2: device[%d]: %s input: %d frame_size: %dx%dpx iomethod: %s %s",
			i,
			inst->device,
			inst->input,
			inst->width,
			inst->height,
			grb_name,
			inst->parent ? "[VIRT] " : "");

		int ret = inst->grb->init(inst);
		if (ret != 0) {
			slog(0, SLOG_ERROR, "fs_v4l2: device[%d] failed to initialize!", i);
			goto epic_fail;
		}

		ret = inst->grb->set_input(inst, inst->input);
		if (ret != 0) {
			slog(0, SLOG_ERROR, "fs_v4l2: Failed to set input %d on device[%d]", i);
			goto epic_fail;
		}

		char *name;
		ret = inst->grb->get_input(inst, &name);
		if (ret < 0) {
			slog(0, SLOG_ERROR, "fs_v4l2: Failed to get current input on device[%d]", i);
			goto epic_fail;
		}
		slog(2, SLOG_INFO, "fs_v4l2: device[%d] input: %d (%s)", i, ret, name);
		free(name);
		inst->initialized = 1;
	}

	//ahttpd_add_path(mpoint, "/frame", dispatch_frame, mpoint);

//	ahttpd_add_path(mpoint, "/fstab", fstab, mpoint);
//	ahttpd_add_path(mpoint, "/terminate", terminate, mpoint);
	return 0;
epic_fail:
	cleanup_devices(data);
	slog(0, SLOG_ERROR, "fs_v4l2: initialization failed, not mounting");
	return -1;
}

static void v4l2_unmount(struct ahttpd_mountpoint *mpoint)
{
	ahttpd_del_path(mpoint, "/frame");
//	ahttpd_del_path(mpoint, "/fstab");

}

static struct ahttpd_fs v4l2fs =
{
	.name = "v4l2",
	.mount = v4l2_mount,
	.unmount = v4l2_unmount,
	.fsdatalen = sizeof(struct v4l2fs_data)
};

AHTTPD_FS(v4l2fs);
