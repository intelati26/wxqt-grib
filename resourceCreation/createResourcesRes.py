#!/usr/bin/env python3
#
# create qrc files res*.qrc for items in res/
#
from typing import List
import glob

bigFiles: List[str] = ["res/hwv4ext.bin", "res/hwv4.bin", "res/lakesv3.bin"]

fh = open("res.qrc", "w")
print('<!DOCTYPE RCC><RCC version="1.0">', file=fh)
print("<qresource>", file=fh)
files: List[str] = glob.glob("res/*")
for f in sorted(files):
    if f not in bigFiles:
        print("    <file>" + f + "</file>", file=fh)
print("</qresource>", file=fh)
print("</RCC>", file=fh)
fh.close()

for index, bigFile in enumerate(bigFiles):
    fh = open("res" + str(index) + ".qrc", "w")
    print('<!DOCTYPE RCC><RCC version="1.0">', file=fh)
    print("<qresource>", file=fh)
    print("    <file>" + bigFile + "</file>", file=fh)
    print("</qresource>", file=fh)
    print("</RCC>", file=fh)
    fh.close()
