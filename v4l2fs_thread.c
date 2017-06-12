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


static void *v4l2_thread(void *arg)
{

}

void v4l2fs_thread_start(v4l2fs_data *data, v4l2fs_grabber *grb, void *arg)
{
    pthread_create(&inst->thread, &inst->thread_attr, lprobe_script_thread, inst);
}

void v4l2fs_thread_stop()
{

}
