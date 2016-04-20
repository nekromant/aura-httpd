# AURA-HTTPD

AURA-HTTPD provides a simple way to bring aura to the world of the WEB automatically creating a fun, easy to use REST API out of the methods you've exported from an aura node. It's still WIP and not yet usable

# Configuration

The configuration file defines the basic things, e.g. what ip address and port to bind to as well as the mountpoints. The mountpoints are URI paths at which different things will appear.

For example, if you mount control at /ctl you will be able to access /ctl/version and /ctl/fstab from your browser. You got the idea.

This section documents all available mount types.

## control

```
{
  "type": "control",
  "mountpoint": "/control",
},
```

A control mountpoint provides a few useful static files.
### version

A JSON-formatted object representing current aura and application version.
example response

```
{
  "aura_version": "0.1.2 bba651c05aec5e2abea95b1d4537f42debc7af04 necromant@sylwer at 19\/04\/16",
  "aura_versioncode": 1002
}
```

### fstab

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

### shutdown

Accessing this file will cause the server to exit within a few seconds.

## static

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
### dir

The directory to serve files from.

### index

Index filename. E.g. index.html. If this file is present in the directory it will be served to the client instead of directory listing.   

### dirlist

Directory listing mode. This can be one of 'none', 'json' or 'html'. None will spit out a 403 error.

## A complete config.json example

```
{
        "host": "127.0.0.1",
        "port": 8088,
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
