#!/bin/bash

. ../test-scripts/common

fetch /
fetch /control/version
fetch /control/fstab
fetch /static/index.html
fetch /static/utils.js

fetch /online/events
fetch /offline/events
fetch /invalid/events

fetch /online/status
fetch /offline/status
fetch /invalid/status

fetch /static/
fetch /static-json/
fetch /static-html/
fetch /static-none/
fetch /static-na/

fetch /control/terminate
exit 0
