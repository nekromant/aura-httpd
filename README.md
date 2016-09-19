[![Build Status](https://jenkins.ncrmnt.org/job/GithubCI/job/aura-httpd/badge/icon)](https://jenkins.ncrmnt.org/job/GithubCI/job/aura-httpd/) [![Coverage Status](https://coveralls.io/repos/github/nekromant/aura-httpd/badge.svg?branch=master)](https://coveralls.io/github/nekromant/aura-httpd?branch=master)
[![Build Status](https://scan.coverity.com/projects/8876/badge.svg)](https://scan.coverity.com/projects/nekromant-aura-httpd)

# AURA-HTTPD

AURA-HTTPD provides a simple way to bring aura to the world of the WEB automatically creating a fun, easy to use REST API out of the methods you've exported from an aura node. It's still WIP and not yet usable

# Configuration

## Top level
The configuration file defines a few of the basic things at the top level. e.g.

```
{
        "host": "127.0.0.1",
        "port": 8088,
        "index": "/static/index.html"
...
```

### host
The ip address to listen on

### port
The port to listen on

### index

Where should the root of the web-server be redirected to. Normally your index is some html document within the web-server, so you would normally want to redirect
the user there.   

## Mount points

The mountpoints are defined in a json array. They are the URI paths at which different things will appear.

For example, if you mount control at /ctl you will be able to access /ctl/version and /ctl/fstab from your browser. Just as if it was a filesystem, plain and simple. You got the idea.

This section documents all available mount types.

### control

```
{
  "type": "control",
  "mountpoint": "/control",
},
```

A control mountpoint provides a few useful static files.
#### version

A JSON-formatted object representing current aura and application version.
example response

```
{
  "aura_version": "0.1.2 bba651c05aec5e2abea95b1d4537f42debc7af04 necromant@sylwer at 19\/04\/16",
  "aura_versioncode": 1002
}
```

#### fstab

Provides information about all mounted filesystems. Basically the 'mountpoints' part of the config file:

```
{
  "fstab": [
    {
      "type": "control",
      "mountpoint": "\/control"
    },
    {
      "type": "node",
      "mountpoint": "\/pickme",
      "transport": "dummy",
      "options": "blah"
    },
    {
      "type": "node",
      "mountpoint": "\/usb",
      "transport": "simpleusb",
      "options": "\/home\/necromant\/.simpleusbconfigs\/pw-ctl.conf"
    },
    {
      "type": "static",
      "mountpoint": "\/static\/",
      "dir": "..\/www",
      "index": "index.html",
      "dirindex": "html"
    }
  ]
}
```

#### shutdown

Accessing this file will cause the server to terminate within a few seconds.

### static

This mountpoint allows you to serve static files. Be warned though - the current static file serving implementation is useful only for testing purposes and MAY be buggy/incomplete/insecure. You are advised to use a full-blown web-server with reverse-proxy functionality to serve static files instead.

```
{
  "type": "static",
  "mountpoint": "/static/",
  "dir": "../www",
  "index": "index.html",
  "dirlist": "html",
},
```
#### dir

The directory to serve files from.

#### index

Index filename. E.g. index.html. If this file is present in the directory it will be served to the client instead of directory listing.   

#### dirlist

Directory listing mode. This can be one of 'none', 'json' or 'html'. None will spit out a 403 error everytime you access a directory and not a file.

### node

This is a very special mountpoint. It represents an aura node that is connected
to the system running aura-httpd. Apart from usual, this requires just 2 more
parameters that are basically the arguments for aura_open(). transport specifies
the name of the transport to use, while options specify the transport module
options.

Example

```
{
    "type": "node",
    "mountpoint": "/dummy",
    "transport":  "dummy",
    "options": "blah",
  },
```

### upload

This mountpoint allows you to upload files via multipart/form-data. This module
has a few variants that differ depending on what you are going to do with the
file. This is specified in the 'mode' parameter

WARNING: Due to how libevent/evhttp works, the whole file being uploaded is
being buffered in system RAM, therefore this will NOT work for uploading huge
files.

#### debug

This mode is basically a boilerplate for new modes development. It prints stuff
when things happen and saves posted data as /tmp/data.bin. Nothing else. When
multiple files are uploaded in one go, each subsequent file overwrites /tmp/data.bin.


```
{
       "type": "upload",
       "mountpoint": "/upload",
       "mode": "debug",
},
```

#### file

This mode stores uploaded files in a directory. Simple as that. You can specify
a directory (that should be writable by the web server) and chose whether to honor original filenames present in Content-Disposition header or not.
If not the files will be stored under random names.

WARNING: Current implementation is very simple. It will overwrite file silently
if it already exists in the directory. Unicode filenames aren't handled
properly.

Example:
```
{
       "type": "upload",
       "mountpoint": "/upload",
       "mode": "file",
       "directory": "/home/necromant/work/aura-httpd/www/upload",
       "original_filename": true,
},
```

#### buffer

This mode allows you to upload a file into an aura_buffer and later pass it as
arguments via aura RPC calls. Every uploaded buffer will be discarded when it is not used for ttl seconds.

```
{
       "type": "upload",
       "mountpoint": "/upload",
       "mode": "buffer",
       "node": "/dummy",
       "ttl": 60,
},
```


## A complete config.json example

```
{
        "host": "127.0.0.1",
        "port": 8088,
        "index": "/static/index.html"
        "mountpoints": [
                 {
                   "type": "control",
                   "mountpoint": "/control",
                 },
                 {
                   "type": "node",
                   "mountpoint": "/pickme",
                   "transport":  "dummy",
                   "options": "blah",
                 },
                 {
                   "type": "node",
                   "mountpoint": "/usb",
                   "transport":  "simpleusb",
                   "options": "/home/necromant/.simpleusbconfigs/pw-ctl.conf",
                 },

                 {
                   "type": "static",
            		   "mountpoint": "/static/",
            		   "dir": "../www",
            		   "index": "index.html2",
            		   "dirlist": "html",
                 },
        ]
}
```

# Known limitations

* File uploads are terribly inefficient and require a lot of RAM
* No authentification mechanisms
* No SSL (If you want SSL - use a reverse proxy)


#TODO
* fs_upload: Implement buffer mode
* fs_node: Rework method calling procedure.  
* fs_v4l2: Implement jpeg file serving from v4l devices
* Implement proper test suite
* Cleanup, valgrinding, static analysis, etc.
