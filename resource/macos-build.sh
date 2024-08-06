#!/bin/sh

if [ -z $1 ]; then
    DIRECTORY="build"
else
    DIRECTORY=$1
fi

if [ -z $2 ]; then
    CONFIGURATION="Debug"
else
    CONFIGURATION=$2
fi

if command -v xcbeautify &> /dev/null; then
    set -o pipefail && cmake --build $DIRECTORY --config $CONFIGURATION | xcbeautify
elif command -v xcpretty &> /dev/null; then
    set -o pipefail && cmake --build $DIRECTORY --config $CONFIGURATION | xcpretty
else
    cmake --build $DIRECTORY --config $CONFIGURATION
fi
