#!/bin/bash

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
TARGET=$SCRIPTPATH/../examples/sandbox/sandbox.ino
BOARD_CONFIG="DxCore:megaavr:avrdb:appspm=no,chip=avr128db48,clock=24internal,bodvoltage=1v9,bodmode=disabled,eesave=enable,resetpin=reset,millis=tcb2,startuptime=8,wiremode=mors2"

if [ -d "$SCRIPTPATH/../build" ]; then
    rm -r "$SCRIPTPATH/../build"
fi

echo "Compiling..."

# Extra args appended to the end, e.g. --clean by the $1 flag
arduino-cli compile "$TARGET" -b $BOARD_CONFIG --output-dir "$SCRIPTPATH/../build" $1
