#!/bin/bash
# builds a test from its main file and the objects of the program itself (make first): the tests then see the same code, settings and libraries as the program does.
#   build_with_app.sh test.cpp output
set -e
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
cd "$root"
defines=$(grep '^DEFINES' Makefile | cut -d= -f2-)
include=$(grep '^INCPATH' Makefile | cut -d= -f2-)
libs=$(grep '^LIBS' Makefile | cut -d= -f2- | sed 's/\$(SUBLIBS)//')
${CXX:-g++} -std=c++20 -fPIC $defines $include -c "$1" -o "$2.o"
${CXX:-g++} "$2.o" $(ls build/release/.obj/*.o | grep -v '/main.o') -o "$2" $libs
