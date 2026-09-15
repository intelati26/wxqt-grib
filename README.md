[TOC]
# wxqt: Qt/C++ port of "wX" US Advanced Weather application (open source GPL3)

This is a fork of [joshua.tee/wxqt](https://gitlab.com/joshua.tee/wxqt) - all
credit for the original app and port goes to Joshua Tee; see below for what's
different here.

[screenshot](https://gitlab.com/joshua.tee/wxqt/-/blob/main/wxqt.png)

Prerequisites:
* Qt 5.12 or higher
* C++ compiler supporting C++17 (most modern Linux distributions are fine)
* Tested on the Linux distros mentioned below, the current version of macOS, and Windows 10. (as of Dec 2021)
```
wxqt is an efficient and configurable method to access weather content from the NWS, NSSL WRF, and blitzortung.org.
Software is provided \"as is\". Use at your own risk. Use for educational purposes and non-commercial purposes only.
Do not use for operational purposes.  Copyright 2020, 2021 joshua.tee@gmail.com .
Privacy Policy: this app does not collect any data from the user or the user’s device.
Please report bugs or suggestions via email."
wxqt is licensed under the GNU GPLv3 license. For more information on the license please go here:"
https://www.gnu.org/licenses/gpl-3.0.en.html
```

## For those interested in forking or running a modified program:
Please modify `GlobalVariables::appName` in `src/common/GlobalVariables.cpp`
(used in HTTP requests to the NWS). The contact email sent alongside it is
no longer hardcoded - it's blank by default and set per-install from
Settings > General > "Contact email for weather API requests," so nothing
personal needs to be edited in source at all.
FYI - you will notice that I've abstracted the native toolkit widgets. This was done as non-public ports to other UI tookits share this codebase, etc.

## Differences from the original Dec 2021 release
  - color legend stays in one spot
  - zoom in/out in nexrad stays over the spot that is centered
  - current device location circle/dot added in nexrad (off by default)
  - nexrad animations are faster
  - can now change the color of the nexrad background

## Differences from mobile versions (similar in content to wXL23 but native desktop with keyboard shortcuts, etc):
- Nexrad Level 2 is not supported. See the wXL23 [FAQ](https://gitlab.com/joshua.tee/wxl23/-/blob/master/doc/FAQ.md#why-is-level-2-radar-not-the-default) for why I can't provide a good experience with this.
- No notifications or widgets
- No Radar color palette editor
- You must compile it yourself
- Best effort support from me (ie Mobile support takes priority)

## How to add your location
- From the main screen, tap the gear icon in the upper left.
- From the Settings window, tap the "Add Location" tab.
- Enter the name of your city in the text box, as type you will start getting matches. The best match will auto populate the name/lat/lon fields.
- If you want a differen result, tap button that most closely matches your location.
- Tap the save button, when all fields clear the new location has been saved.
- Close settings window
- From the main screen use the drop down to choose your new location

## FYI - output to local filesystem (file should NOT exist before running program for first time):
- MacOS (standard pref spot): `$HOME/Library/Preferences/com.tee@gmail.joshua.wxqt.plist`
- Linux distro: `$HOME/.config/joshua.tee@gmail.com/wxqt.conf`

## Bugs (that might never get fixed)
* In Nexrad when zooming out of in, it does not stay centered.
* On initial nexrad launch it is not centered on radar site. Usage after this is fine.
* At times if a thread gets stuck it will not exit properly, **recommendation is to always start program from command line**.

## Help
From the main screen and nexrad radar do `Ctrl-/` (? key) to get keyboard shortcuts. Mouse over on some icons will sometimes show a label or shortcut as well.

## Compile and run
1. Perform the [steps](https://gitlab.com/joshua.tee/wxqt/-/blob/main/README_OS.md) for your operating system, you will probably need 8GB of memory for compilation. I have used a 4GB Raspberry PI 400 (keyboard model) to compile.
2. Download the code and and compile/run
```bash
git clone https://gitlab.com/joshua.tee/wxqt.git
cd wxqt
# NOTE: Windows requires additional steps, please see README_WINDOWS.md
./makeAll.py --qt5
# or for qt6
./makeAll.py
```
3. After compilation you can simply launch with script
```bash
./run.bash
```
## Qt 6 note (TODO):
- need to add instructions but use "qmake6" instead
- need 
```bash
qt6-base
qt6-declarative
