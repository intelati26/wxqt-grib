#!/bin/sh
#
# compile and run the program
# compiles with a job for each core that the free memory can hold, unless called with an arg
#

appName="wxqt"

ulimit -c unlimited
FILE=core
if test -f "$FILE"; then
    rm $FILE
fi

# one job per core, but no more than the free memory allows (a compile takes up to about 0.9 GB); an argument sets the number
jobs=4
if command -v nproc >/dev/null 2>&1; then
    jobs=$(nproc)
    if [ -r /proc/meminfo ]; then
        byMemory=$(awk '/MemAvailable/ {print int($2 / 1048576 / 0.9)}' /proc/meminfo)
        if [ "$byMemory" -ge 2 ] && [ "$byMemory" -lt "$jobs" ]; then
            jobs=$byMemory
        fi
    fi
fi
buildCommand="make -j ${jobs}"
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
