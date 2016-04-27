#!/bin/bash

. ../test-scripts/common

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


fetch /control/terminate
