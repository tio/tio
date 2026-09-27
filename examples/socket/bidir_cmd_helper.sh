#!/bin/sh

if [ $# -lt 2 ]; then
    echo "Usage: $0 <socket-path> <bidirectional-command>"
    echo "Example: $0 /tmp/tio-socket0 \"sz -b -p sample.bin\""
    echo "Note: Please run \"tio -S <socket-path> <your-serial-device>\" beforehand."
    exit 1
fi

SOCKET_PATH=$1
BIDIR_CMD=$2

socat EXEC:"$BIDIR_CMD" $SOCKET_PATH

exit $?
