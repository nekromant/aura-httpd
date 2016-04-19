# AURA-HTTPD

AURA-HTTPD provides a simple way to bring aura to the world of the WEB automatically creating a fun, easy to use REST API out of the methods you've exported from an aura node. It's still WIP and not yet usable

## config.json

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
                   "mountpoint": "/",
                   "dir": "../www",
                 },
        ]
}
```
