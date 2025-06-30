#!/bin/sh
#
# compile and run the program
# compiles with 4 cores unless called with an arg
#

appName="wxqt"

ulimit -c unlimited
FILE=core
if test -f "$FILE"; then
    rm $FILE
fi

buildCommand="make -j 4"
if [ "$1" != "" ]; then
    buildCommand="make -j ${1}"
fi
echo ${buildCommand}

if ${buildCommand}; then
    if [ "$(uname)" = "Darwin" ]; then
        build/release/${appName}.app/Contents/MacOS/${appName} "$@"
    else
        build/release/${appName} "$@"
    fi
else
    compilation failed
fi
