#!/bin/bash
#
# package-windows.sh
#
# Builds a self-contained "portable" folder for wxqt on Windows: copies
# wxqt.exe plus every Qt DLL/plugin, MinGW runtime DLL, and (if installed)
# GDAL tool/DLL/data file it needs, so the folder runs on a plain Windows
# install with none of that already present - no installer, just unzip and
# run wxqt.bat.
#
# Run from an MSYS2 "MinGW64" shell, from the wxqt checkout root, after a
# successful build (build/release/wxqt.exe must exist). Used by
# .github/workflows/build.yml's windows job; also usable by hand the same
# way (see README_WINDOWS.md).
#
set -e

if [ "$MSYSTEM" != "MINGW64" ]; then
    echo "error: run this from the 'MSYS2 MinGW64' shell (Start menu), not MSYS/UCRT64/CLANG64." >&2
    echo "       current MSYSTEM=$MSYSTEM" >&2
    exit 1
fi

exeSrc="build/release/wxqt.exe"
if [ ! -f "$exeSrc" ]; then
    echo "error: $exeSrc not found - build it first (./run.bash or make)." >&2
    exit 1
fi

distDir="dist/wxqt-portable"
echo "==> packaging into $distDir"
rm -rf "$distDir"
mkdir -p "$distDir"
cp "$exeSrc" "$distDir/wxqt.exe"

# ---------------------------------------------------------------------------
# Qt DLLs, plugins, and (automatically, since wxqt links QtWebEngineWidgets)
# the QtWebEngineProcess.exe + resources/ + translations/qtwebengine_locales
# that Chromium needs. --compiler-runtime also pulls in the MinGW/GCC runtime
# DLLs (libstdc++-6.dll, libgcc_s_seh-1.dll, libwinpthread-1.dll).
# ---------------------------------------------------------------------------
windeployqt="/mingw64/bin/windeployqt6.exe"
if [ ! -x "$windeployqt" ]; then
    windeployqt="/mingw64/bin/windeployqt.exe"   # older MSYS2 releases ship it unsuffixed
fi
if [ ! -x "$windeployqt" ]; then
    echo "error: windeployqt6 not found - install it with:" >&2
    echo "       pacman -S mingw-w64-x86_64-qt6-tools" >&2
    exit 1
fi
echo "==> running $windeployqt"
"$windeployqt" --release --compiler-runtime "$distDir/wxqt.exe"

# ---------------------------------------------------------------------------
# windeployqt only chases down Qt's own DLL dependency tree. It's generally
# good about plugin deps too, but this sweep catches anything left over -
# any /mingw64/bin/*.dll referenced (directly or transitively) by anything
# already in distDir that isn't there yet. Repeats until nothing new turns
# up, so it also picks up dependencies-of-dependencies.
# ---------------------------------------------------------------------------
sweepDir() {
    local dir="$1"
    local added=1
    while [ "$added" -eq 1 ]; do
        added=0
        while IFS= read -r bin; do
            for dep in $(ldd "$bin" 2>/dev/null | awk '{print $3}'); do
                case "$dep" in
                    /mingw64/bin/*)
                        local name
                        name="$(basename "$dep")"
                        if [ ! -f "$dir/$name" ]; then
                            cp "$dep" "$dir/"
                            added=1
                        fi
                        ;;
                esac
            done
        done < <(find "$dir" -maxdepth 1 -type f \( -name '*.exe' -o -name '*.dll' \))
        # also sweep nested plugin folders (platforms/, styles/, etc.) for deps,
        # copying any missing ones into the top-level dir (Windows checks the
        # main exe's own folder before a plugin's folder when resolving a DLL)
        while IFS= read -r bin; do
            for dep in $(ldd "$bin" 2>/dev/null | awk '{print $3}'); do
                case "$dep" in
                    /mingw64/bin/*)
                        local name2
                        name2="$(basename "$dep")"
                        if [ ! -f "$dir/$name2" ]; then
                            cp "$dep" "$dir/"
                            added=1
                        fi
                        ;;
                esac
            done
        done < <(find "$dir" -mindepth 2 -type f \( -name '*.dll' \))
    done
}

echo "==> sweeping for additional MinGW runtime DLLs"
sweepDir "$distDir"

# ---------------------------------------------------------------------------
# GDAL - used by the RRFS GRIB viewer and the SPC Post slideshow viewer via
# QStandardPaths::findExecutable("gdalwarp"), i.e. found through PATH at
# runtime, not linked into wxqt.exe itself. Only bundled if it's actually
# installed in this MSYS2 environment; otherwise those two viewers will
# report "GDAL not found" in the packaged app.
# ---------------------------------------------------------------------------
if [ -f /mingw64/bin/gdalwarp.exe ]; then
    echo "==> GDAL found - bundling gdalwarp/gdaldem/gdal_translate/gdal_rasterize/gdalinfo"
    mkdir -p "$distDir/gdal"
    for tool in gdalwarp gdaldem gdal_translate gdal_rasterize gdalinfo; do
        [ -f "/mingw64/bin/$tool.exe" ] && cp "/mingw64/bin/$tool.exe" "$distDir/gdal/"
    done
    # NOTE: gdal_calc (used by the RRFS viewer's Celsius->Fahrenheit rescale)
    # is a Python script requiring a full python + osgeo/gdal bindings - not
    # practical to bundle portably, so it's intentionally left out. Without
    # it, UtilityGrib::render()'s Fahrenheit conversion step is skipped and
    # falls back to showing Celsius for temperature-in-F fields; nothing else
    # is affected (the SPC Post viewer never calls gdal_calc).
    sweepDir "$distDir/gdal"
    [ -d /mingw64/share/gdal ] && cp -r /mingw64/share/gdal "$distDir/gdal/gdal-data"
    [ -d /mingw64/share/proj ] && cp -r /mingw64/share/proj "$distDir/gdal/proj-data"
else
    echo "==> note: GDAL not installed in this MSYS2 env - skipping."
    echo "    without it, the GRIB and SPC Post viewers will report 'GDAL not found'."
fi

# ---------------------------------------------------------------------------
# launcher - puts the bundled gdal/ folder on PATH and points GDAL/PROJ at
# their bundled data dirs before starting wxqt.exe. Launch via this, not
# wxqt.exe directly, so the bundled GDAL tools (if present) are found.
# ---------------------------------------------------------------------------
cat > "$distDir/wxqt.bat" << 'BATEOF'
@echo off
setlocal
set "HERE=%~dp0"
if exist "%HERE%gdal" (
    set "PATH=%HERE%gdal;%PATH%"
    set "GDAL_DATA=%HERE%gdal\gdal-data"
    set "PROJ_DATA=%HERE%gdal\proj-data"
)
start "" "%HERE%wxqt.exe" %*
BATEOF

echo "==> done"
echo "    portable folder: $distDir  ($(du -sh "$distDir" | cut -f1))"
