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

For example, if you mount control at /ctl you will be able to access /ctl/version and /ctl/fstab from your browser. You got the idea.

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

Accessing this file will cause the server to exit within a few seconds.

### static

This mountpoint allows you to serve static files. Be warned though - the current static file serving implementation is useful only for testing purposes and MAY be buggy/incomplete/insecure. You are adviced to use a full-blown web-server with reverse-proxy functionality instead.

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

Directory listing mode. This can be one of 'none', 'json' or 'html'. None will spit out a 403 error.

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


#TODO

* main: move server creation/destruction functions to a separate file
* main: Commandline arguments parsing
* main: Daemonize properly
* fs_node: Rework method calling procedure.  
* Properly cleanup and free resources on exit
* fs_v4l2: Implement jpeg file serving from v4l devices
* Add CI builds and tests
* Implement test suite
* Cleanup, valgrinding, static analysis, etc.
* systemd unit file
