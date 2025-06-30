[TOC]
### Windows
* Is documented in a standalone [file](https://gitlab.com/joshua.tee/wxqt/-/blob/main/README_WINDOWS.md)
### Debian 11
```bash
sudo apt install g++
sudo apt install qt5-qmake qtbase5-dev qtpositioning5-dev qtwebengine5-dev
```
### Ubuntu 20.4, Linux Mint 20.4
```bash
sudo apt install g++ make
sudo apt install qt5-qmake
sudo apt install qt5-default
sudo apt install qtpositioning5-dev qtwebengine5-dev
```
### Ubuntu 21.04 onwards:
```bash
sudo apt-get install make g++ qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools qtpositioning5-dev qtwebengine5-dev
```
### elementary OS 6
```bash
sudo apt install elementary-sdk
sudo apt install qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools qtpositioning5-dev qtwebengine5-dev
```
### Fedora 35 onwards (bug: windows are not maximized on launch):
```bash
sudo yum install qt5-qtbase-devel
```
### Manjaro 21.1.6+ (KDE):
```bash
sudo pacman -S make
sudo pacman -S gcc
# qt5
sudo pacman -S qt5-location
# qt6
sudo pacman -S qt6-base
make clean
qmake6
make -j 4
```
### MacOS
You'll need to be ok with installing (brew)[https://brew.sh/] on your system.
Another option is to install Qt directly from the vendor: (The Qt Company)[https://www.qt.io/download]
```bash
brew install qt
```
### chromeOS
```
sudo apt install g++
sudo apt install make
# qt 5
sudo apt install qtbase5-dev

# if available
sudo apt install qt6-base-dev qt6-webengine-dev
```
### Raspbian GNU/Linux 10 (buster)
```bash
# unfortunately I did not record exactly what I installed
# but I think the following should work
sudo apt install g++
sudo apt install qt5-qmake qtbase5-dev
sudo apt install libqt5positioning5
sudo apt install qtpositioning5-dev qtwebengine5-dev


#
# NOTE for systems with less then 4GB RAM
# you should run the compile script with a call to use only one processing core
# as several very large source files can only compile one at a time
# instead of
./makeAll.py
# run the following to indicate to use 1 core onstead of 4 cores
./makeAll.py 1

#
# if this still does not work, try allocate 2GB swap
#
sudo fallocate -l 2G /root/twoGigSwap
sudo mkswap /root/twoGigSwap
sudo swapon /root/twoGigSwap

rm build/release/.obj/qrc_*
make

sudo swapoff /root/twoGigSwap
./run.bash
```
### Raspbian GNU/Linux 12 (bookworm)
```
# sudo apt install g++
sudo apt install make
sudo apt install qt6-base-dev qt6-webengine-dev
# use qt6
./makeAll.py
```
### FreeBSD 14
```
pkg install python3
pkg install qt6
# use qt6
./makeAll.py
```
