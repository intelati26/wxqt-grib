#!/usr/bin/env python3
#
#
# build project from scratch
#
# TODO FIXME options to rebuild meson only and resources
#
# if downloading zip for git clone for first time
# run this to fully setup and run
# after, just run "./run.bash"
#

import argparse
import glob
import os.path
import os
from pathlib import Path
import shutil
import sys
import subprocess
from subprocess import Popen, PIPE
from typing import Tuple, List

# positioning
proHeader: str = """QT += core gui network widgets webenginewidgets
CONFIG += c++2a
DEFINES += QT_DEPRECATED_WARNINGS
INCLUDEPATH += src

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG(debug, debug|release) {
    DESTDIR = build/debug
}
CONFIG(release, debug|release) {
    DESTDIR = build/release
}

OBJECTS_DIR = $$DESTDIR/.obj
MOC_DIR = $$DESTDIR/.moc
RCC_DIR = $$DESTDIR/.qrc
UI_DIR = $$DESTDIR/.u

SOURCES += \\
"""

proFooter: str = """
# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
"""


#
# helper function to run command
#
def runMe(cmd: str) -> Tuple[str, str, int]:
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
    output, err = p.communicate()
    exitCode = p.wait()
    return output.decode("utf-8"), err.decode("utf-8"), exitCode


def run(command: str):
    process = Popen(command, stdout=PIPE, shell=True)
    while True:
        assert process.stdout is not None
        line = process.stdout.readline().decode("utf-8").rstrip()
        if not line:
            break
        yield line


def makePro(extraFooter: str = "\n") -> None:
    cppFiles: List[str] = glob.glob("src/*.cpp") + glob.glob("src/*/*.c*")
    headerFiles: List[str] = glob.glob("src/*/*.h")
    proTargetFile: str = "wxqt.pro"
    with open(proTargetFile, "w") as fh:
        fh.write(proHeader)
        for cpp in sorted(cppFiles):
            fh.write(" " * 4 + cpp + " \\" + '\n')

        fh.write("\nHEADERS += \\" + "\n")
        for f in sorted(headerFiles):
            fh.write(" " * 4 + f + " \\" + '\n')

        # RESOURCES = resourceCreation/res.qrc resourceCreation/images.qrc
        qrcFiles: List[str] = glob.glob("resourceCreation/*.qrc")
        fh.write("\nRESOURCES += \\" + "\n")
        for qrc in sorted(qrcFiles):
            fh.write(" " * 4 + qrc + " \\" + '\n')

        fh.write(proFooter)
        fh.write(extraFooter)


if __name__ == "__main__":
    # for path in run("ping -c 5 google.com"):
    #     print(path)

    parser = argparse.ArgumentParser()
    parser.add_argument('--singleThread', action='store_true')
    parser.add_argument('--pro', action='store_true')
    parser.add_argument('--qt5', action='store_true')
    parser.add_argument('--gcc', action='store_true')
    args = parser.parse_args()

    #
    # "build" dir - remove
    #
    if not args.pro:
        buildDir: str = "build"
        if Path(buildDir).is_dir():
            shutil.rmtree(buildDir)

    #
    # resource creation
    #
    if not args.pro:
        os.chdir("resourceCreation")
        out, err, returnCode = runMe("./createResourcesImage.py")
        out, err, returnCode = runMe("./createResourcesRes.py")
        os.chdir("..")

    #
    # make pro file
    #
    if not args.gcc:
        makePro()
    else:
        makePro("QMAKE_CXX = g++\n")

    #
    # configure with qmake
    #
    qmakeCommand: str = "qmake6"

    osfile: str = "/etc/os-release"
    if os.path.exists(osfile):
        osFileData = open(osfile).read()
        if "Fedora Linux" in osFileData or "FreeBSD" in osFileData:
            qmakeCommand = "qmake-qt5"
        elif "MSYS2" in osFileData:
            qmakeCommand = "qmake6"

    if args.qt5:
        qmakeCommand = "qmake"

    out, err, returnCode = runMe(qmakeCommand)
    print(qmakeCommand, ":", out, err)
    if returnCode != 0:
        print("exit on qmake failure")
        sys.exit(returnCode)

    if args.pro:
        sys.exit(0)

    #
    # build project
    #
    runArg: str = ""
    if args.singleThread:
        runArg = " 1"
    os.system(os.getcwd() + "/run.bash" + runArg)
    # out, err, returnCode = runMe("./run.bash" + runArg)
    # print(returnCode, ":", out + err)
