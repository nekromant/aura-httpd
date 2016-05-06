#include <stdio.h>
#include <search.h>
#include <stdlib.h>
#include <string.h>
#include <aura/aura.h>
#include <aura/private.h>

#define ARRAY_SIZE(a) ((sizeof(a) / sizeof(*(a))))

static ENTRY mimedb[] = {
	{ ".pdf",     "application/pdf",					  },
	{ ".sig",     "application/pgp-signature",				  },
	{ ".spl",     "application/futuresplash",				  },
	{ ".class",   "application/octet-stream",				  },
	{ ".ps",      "application/postscript",					  },
	{ ".torrent", "application/x-bittorrent",				  },
	{ ".dvi",     "application/x-dvi",					  },
	{ ".gz",      "application/x-gzip",					  },
	{ ".pac",     "application/x-ns-proxy-autoconfig",			  },
	{ ".swf",     "application/x-shockwave-flash",				  },
	{ ".tar.gz",  "application/x-tgz",					  },
	{ ".tgz",     "application/x-tgz",					  },
	{ ".tar",     "application/x-tar",					  },
	{ ".zip",     "application/zip",					  },
	{ ".mp3",     "audio/mpeg",						  },
	{ ".m3u",     "audio/x-mpegurl",					  },
	{ ".wma",     "audio/x-ms-wma",						  },
	{ ".wax",     "audio/x-ms-wax",						  },
	{ ".ogg",     "application/ogg",					  },
	{ ".wav",     "audio/x-wav",						  },
	{ ".gif",     "image/gif",						  },
	{ ".jpg",     "image/jpeg",						  },
	{ ".jpeg",    "image/jpeg",						  },
	{ ".png",     "image/png",						  },
	{ ".xbm",     "image/x-xbitmap",					  },
	{ ".xpm",     "image/x-xpixmap",					  },
	{ ".xwd",     "image/x-xwindowdump",					  },
	{ ".css",     "text/css",						  },
	{ ".html",    "text/html",						  },
	{ ".htm",     "text/html",						  },
	{ ".js",      "text/javascript",					  },
	{ ".asc",     "text/plain",						  },
	{ ".c",	      "text/plain",						  },
	{ ".cpp",     "text/plain",						  },
	{ ".log",     "text/plain",						  },
	{ ".conf",    "text/plain",						  },
	{ ".text",    "text/plain",						  },
	{ ".txt",     "text/plain",						  },
	{ ".spec",    "text/plain",						  },
	{ ".dtd",     "text/xml",						  },
	{ ".xml",     "text/xml",						  },
	{ ".mpeg",    "video/mpeg",						  },
	{ ".mpg",     "video/mpeg",						  },
	{ ".mov",     "video/quicktime",					  },
	{ ".qt",      "video/quicktime",					  },
	{ ".avi",     "video/x-msvideo",					  },
	{ ".asf",     "video/x-ms-asf",						  },
	{ ".asx",     "video/x-ms-asf",						  },
	{ ".wmv",     "video/x-ms-wmv",						  },
	{ ".bz2",     "application/x-bzip",					  },
	{ ".tbz",     "application/x-bzip-compressed-tar",			  },
	{ ".tar.bz2", "application/x-bzip-compressed-tar",			  },
	{ ".odt",     "application/vnd.oasis.opendocument.text",		  },
	{ ".ods",     "application/vnd.oasis.opendocument.spreadsheet",		  },
	{ ".odp",     "application/vnd.oasis.opendocument.presentation",	  },
	{ ".odg",     "application/vnd.oasis.opendocument.graphics",		  },
	{ ".odc",     "application/vnd.oasis.opendocument.chart",		  },
	{ ".odf",     "application/vnd.oasis.opendocument.formula",		  },
	{ ".odi",     "application/vnd.oasis.opendocument.image",		  },
	{ ".odm",     "application/vnd.oasis.opendocument.text-master",		  },
	{ ".ott",     "application/vnd.oasis.opendocument.text-template",	  },
	{ ".ots",     "application/vnd.oasis.opendocument.spreadsheet-template",  },
	{ ".otp",     "application/vnd.oasis.opendocument.presentation-template", },
	{ ".otg",     "application/vnd.oasis.opendocument.graphics-template",	  },
	{ ".otc",     "application/vnd.oasis.opendocument.chart-template",	  },
	{ ".otf",     "application/vnd.oasis.opendocument.formula-template",	  },
	{ ".oti",     "application/vnd.oasis.opendocument.image-template",	  },
	{ ".oth",     "application/vnd.oasis.opendocument.text-web",		  },
	/* Sentinel */
	{ NULL, NULL },
};

struct hsearch_data *ahttpd_mime_init()
{
	ENTRY *ep;
	struct hsearch_data *hs = calloc(1, sizeof(*hs));
	int i = 0;
	int ret;
	if (!hs)
		BUG(NULL, "Failed to allocate memory");
	int n = ARRAY_SIZE(mimedb);
	hcreate_r(n+n/4, hs);
	while (mimedb[i].key) {
		ret = hsearch_r(mimedb[i], ENTER, &ep, hs);
		if (!ret)
			BUG(NULL, "Error inserting mime data");
		i++;
	}
	slog(2, SLOG_INFO, "Done generating mime index: %d entries", n);
	return hs;
}

static const char *file_get_extension(const char *filename)
{
	return strrchr(filename, '.');
}

/**
 * Guess the file mimetype from filename. The string should not be freed.
 *
 * @param  instance mimedb instance
 * @param  filename filename to search against
 * @return          The guessed mimetype.
 */
const char *ahttpd_mime_guess(struct hsearch_data *instance, const char *filename)
{
	const char *ext = file_get_extension(filename);
	int ret;
	ENTRY ep;
	ENTRY *found;

	if (!ext)
		goto bailout;

	ep.key = (char *) ext;
	ep.data = NULL;
	ret = hsearch_r(ep, FIND, &found, instance);
	if (ret)
		return found->data;
bailout:
		return "application/octet-stream";
}

void ahttpd_mime_destroy(struct hsearch_data *instance)
{
	hdestroy_r(instance);
	free(instance);
}
