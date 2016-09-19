#include <aura/aura.h>
#include <evhttp.h>
#include <json.h>
#include <stdio.h>
#include <stdbool.h>
#include <dirent.h>
#include <stdlib.h>
#include <fcntl.h>
#include <http_parser.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <aura-httpd/utils.h>
#include <aura-httpd/uploadfs.h>


struct filemod_data {
	bool			original_filename;
	char *			directory;
	char *			filename;
	struct json_object *	reply;
};

static void kill_trailing_slash(char *str)
{
	if (str[strlen(str) - 1] == '/')
		str[strlen(str) - 1] = 0x0;
}

static int file_init(struct upfs_data *fsd)
{
	struct json_object *props = fsd->mpoint->props;
	struct filemod_data *fdata = calloc(sizeof(*fdata), 1);

	if (!fdata)
		return -ENOMEM;
	fsd->mod_data = fdata;

	const char *directory = json_find_string(props, "directory");
	if (!directory) {
		slog(0, SLOG_ERROR, "Missing proper upload directory");
		return -EBADMSG;
	}

	fdata->directory = strdup(directory);
	if (!fdata->directory)
		goto err_free_fdata;

	kill_trailing_slash(fdata->directory);
	fdata->original_filename = json_find_boolean(props, "original_filename");

	slog(4, SLOG_DEBUG, "fs_upload_mod_file: directory %s original_filename: %s",
	     fdata->directory,
	     fdata->original_filename ? "true" : "false"
	     );

	char *tmpfpath;

	if (-1 == asprintf(&tmpfpath, "%s/ahttpd_write_check_XXXXXX", fdata->directory))
		goto err_free_dir;

	int fd = mkstemp(tmpfpath);
	if (fd == -1) {
		slog(0, SLOG_ERROR, "Server can't write to %s (Failed to create %s)",
		     fdata->directory, tmpfpath);
		goto err_free_tmpfpath;
	}
	close(fd);
	unlink(tmpfpath);
	free(tmpfpath);
	/* At this point we know everything we should */
	return 0;

err_free_tmpfpath:
	free(tmpfpath);
err_free_dir:
	free(fdata->directory);
err_free_fdata:
	free(fdata);
	return -EIO;
}

static void file_deinit(struct upfs_data *fsd)
{
	struct filemod_data *fdata = fsd->mod_data;

	free(fdata->directory);
	free(fdata);
}

static void file_handle_rq_headers(struct upfs_data *fsd)
{
	struct filemod_data *fdata = fsd->mod_data;

	if (fdata->filename) {
		free(fdata->filename);
		fdata->filename = NULL;
	}
	if (fdata->reply)
		json_object_put(fdata->reply);

	fdata->reply = json_object_new_array();
	if (!fdata->reply)
		uploadfs_upload_send_error(fsd, NULL);
}

static char *get_original_filename(char *cds_string)
{
	char *ret = malloc(strlen(cds_string));

	if (!ret)
		return NULL;

	char tmp[] = "filename=\"";
	char *pos = strstr(cds_string, tmp);
	pos = &pos[strlen(tmp)];
	int i = 0;
	while ((*pos) && (*pos != '\"'))
		ret[i++] = *pos++;
	ret[i] = 0x0;
	for (i = 0; i < strlen(ret); i++)
		if (ret[i] == '/')
			ret[i] = '_';

	return ret;
}

static void file_handle_form_header(struct upfs_data *fsd, char *key, char *value)
{
	struct filemod_data *fdata = fsd->mod_data;

	if (!fdata->original_filename)
		return;
	if (strcmp(key, "Content-Disposition") == 0) {
		fdata->filename = get_original_filename(value);
		if ((!fdata->filename) || (!(strlen(fdata->filename))))
			slog(0, SLOG_ERROR, "Failed to get filename from Content-Disposition header");
		uploadfs_upload_send_error(fsd, NULL);
	}
}

static void file_handle_data(struct upfs_data *fsd, struct evbuffer_iovec *vec, int length)
{
	struct filemod_data *fdata = fsd->mod_data;
	char *filepath;
	char *filename;
	int ret;
	char randname[32];
	struct json_object *tmp;
	struct json_object *fl = json_object_new_object();


	if (!fl)
		goto error;

	sprintf(randname, "up.%ld", random());

	if (fdata->original_filename)
		filename = fdata->filename;
	else
		filename = randname;

	ret = asprintf(&filepath, "%s/%s", fdata->directory, filename);
	if (ret == -1)
		goto error;

	tmp = json_object_new_string(filepath);
	if (!tmp)
		goto error;
	json_object_object_add(fl, "fullpath", tmp);

	tmp = json_object_new_string(filename);
	if (!tmp)
		goto error;
	json_object_object_add(fl, "filename", tmp);
	json_object_array_add(fdata->reply, fl);
	fl = NULL; /* We don't own it any more */

	if (0 != dump_iovec_to_file(filepath, vec, length))
		goto error;

	return;
error:
	if (fl)
		json_object_put(fl);
	uploadfs_upload_send_error(fsd, NULL);
}

static void file_send_result(struct upfs_data *fsd)
{
	struct filemod_data *fdata = fsd->mod_data;

	ahttpd_reply_with_json(fsd->request, fdata->reply);
	json_object_put(fdata->reply);
	fdata->reply = NULL;
	return;
}

static struct uploadfs_module filemod = {
	.name			= "file",
	.init			= file_init,
	.deinit			= file_deinit,
	.inbound_request_hook	= file_handle_rq_headers,
	.handle_form_header	= file_handle_form_header,
	.handle_data		= file_handle_data,
	.send_upload_reply	= file_send_result,
};

AHTTPD_UPLOADFS_MODULE(filemod);
