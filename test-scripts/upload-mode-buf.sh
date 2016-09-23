#!/bin/bash

. ../test-scripts/common

upload_shit upload_to_dummy
fetch upload_to_dummy/
fetch upload_to_dummy/download/8
download /upload_to_dummy/download/0 /tmp/dowloaded_shit.bin
cmp /tmp/shit.bin /tmp/dowloaded_shit.bin
fetch upload_to_dummy/drop/0
fetch /control/terminate
exit 0
