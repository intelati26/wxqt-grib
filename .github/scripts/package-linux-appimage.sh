#!/bin/bash
# Packages a just-built build/release/wxqt into a single-file AppImage.
# Run on a plain ubuntu-latest GitHub Actions runner - NOT meant for local
# use on a CPU-optimized distro (e.g. CachyOS/znver4 builds): those use
# IFUNC resolvers that crash when their .so files are copied elsewhere and
# loaded via LD_LIBRARY_PATH instead of their original system path. A
# generic runner has no such issue.
#
# Deliberately does NOT use linuxdeploy for the executable/plugin bundling
# step (tried, found real breakage on a bleeding-edge system: a stale
# bundled `strip` choking on modern RELR relocations, then a qt.conf/
# plugin-loading path that segfaulted even after working around that) -
# appimagetool alone, fed a manually-assembled AppDir, is used instead. It
# only squashes a directory into a single file; it doesn't patch or strip
# anything inside it, so there's much less for a stale bundled tool to get
# wrong.
set -ex

appDir="$PWD/AppDir"
rm -rf "$appDir"
mkdir -p "$appDir/usr/bin" "$appDir/usr/lib" "$appDir/usr/plugins" \
         "$appDir/usr/gdal/bin" "$appDir/usr/gdal/lib" \
         "$appDir/usr/libexec" "$appDir/usr/resources"

cp build/release/wxqt "$appDir/usr/bin/wxqt"
cp resourceCreation/images/wx_launcher.png "$appDir/wxqt.png"
cat > "$appDir/wxqt.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=wxqt
Exec=wxqt
Icon=wxqt
Categories=Utility;
Terminal=false
EOF

# KNOWN GAP (narrowed): GRIB, SPC Post and REFS rendering no longer need
# gdal_calc/gdal_merge (Python scripts, not bundled). Only the SHIP/STP
# severe-indices viewer still uses gdal_calc for its formula math, so that
# viewer will not render from this AppImage until ported off Python (see
# docs/outstanding-work.md).
for tool in gdalwarp gdaldem gdal_translate gdal_rasterize gdalinfo; do
    cp "$(command -v "$tool")" "$appDir/usr/gdal/bin/"
done
cp -r /usr/share/gdal "$appDir/usr/gdal/share-gdal-data"
cp -r /usr/share/proj "$appDir/usr/gdal/share-proj-data"

# Copies every shared-library dependency (direct + transitive) of every
# binary already in $1 into $1 itself, repeating until nothing new turns
# up. Skips the small set of core libs every target Linux machine is
# assumed to already have (glibc/libstdc++'s own runtime) - a mismatched
# bundled libc/libstdc++ is exactly the kind of crash this whole approach
# is trying to avoid, and a newer host glibc is required to run a binary
# built on a given glibc version anyway.
sweep() {
    local dir="$1"
    local added=1
    while [ "$added" -eq 1 ]; do
        added=0
        while IFS= read -r bin; do
            while IFS= read -r dep; do
                [ -z "$dep" ] && continue
                case "$dep" in
                    */libc.so*|*/libm.so*|*/libpthread.so*|*/libdl.so*|*/librt.so*|*/libutil.so*|*/libstdc++.so*|*/libgcc_s.so*)
                        continue ;;
                esac
                local name
                name="$(basename "$dep")"
                if [ ! -f "$dir/$name" ] && [ -f "$dep" ]; then
                    cp "$dep" "$dir/"
                    added=1
                fi
            done < <(ldd "$bin" 2>/dev/null | grep ' => ' | awk '{print $3}')
        done < <(find "$dir" -maxdepth 1 -type f -executable)
    done
}

cp "$appDir/usr/bin/wxqt" "$appDir/usr/lib/wxqt-for-sweep"
sweep "$appDir/usr/lib"
rm "$appDir/usr/lib/wxqt-for-sweep"

sweep "$appDir/usr/gdal/bin"
for so in "$appDir"/usr/gdal/bin/*.so*; do
    [ -e "$so" ] && mv "$so" "$appDir/usr/gdal/lib/"
done

pluginsDir="/usr/lib/x86_64-linux-gnu/qt6/plugins"
for sub in platforms imageformats tls iconengines; do
    [ -d "$pluginsDir/$sub" ] && cp -r "$pluginsDir/$sub" "$appDir/usr/plugins/"
done
sweep "$appDir/usr/plugins"
sweep "$appDir/usr/lib"

webengineDir="$(find /usr -maxdepth 6 -type d -name libexec 2>/dev/null | head -1)"
[ -n "$webengineDir" ] && cp -r "$webengineDir"/. "$appDir/usr/libexec/"
for f in icudtl.dat qtwebengine_resources.pak qtwebengine_resources_100p.pak qtwebengine_resources_200p.pak qtwebengine_devtools_resources.pak; do
    found="$(find /usr -maxdepth 8 -name "$f" 2>/dev/null | head -1)"
    [ -n "$found" ] && cp "$found" "$appDir/usr/resources/"
done
localesDir="$(find /usr -maxdepth 8 -type d -name qtwebengine_locales 2>/dev/null | head -1)"
[ -n "$localesDir" ] && cp -r "$localesDir" "$appDir/usr/resources/"

cat > "$appDir/AppRun" << 'EOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "${0}")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:$HERE/usr/gdal/lib:${LD_LIBRARY_PATH}"
export PATH="$HERE/usr/gdal/bin:${PATH}"
export QT_PLUGIN_PATH="$HERE/usr/plugins"
export GDAL_DATA="$HERE/usr/gdal/share-gdal-data"
export PROJ_DATA="$HERE/usr/gdal/share-proj-data"
export PROJ_LIB="$HERE/usr/gdal/share-proj-data"
if [ -d "$HERE/usr/libexec" ]; then
    export QTWEBENGINEPROCESS_PATH="$(find "$HERE/usr/libexec" -name QtWebEngineProcess | head -1)"
fi
if [ -d "$HERE/usr/resources" ]; then
    export QTWEBENGINE_RESOURCES_PATH="$HERE/usr/resources"
    export QTWEBENGINE_LOCALES_PATH="$HERE/usr/resources/qtwebengine_locales"
fi
exec "$HERE/usr/bin/wxqt" "$@"
EOF
chmod +x "$appDir/AppRun"

wget -q -O /tmp/appimagetool https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x /tmp/appimagetool

mkdir -p dist
ARCH=x86_64 /tmp/appimagetool --appimage-extract-and-run "$appDir" dist/wxqt-x86_64.AppImage

echo "==> built: dist/wxqt-x86_64.AppImage ($(du -sh dist/wxqt-x86_64.AppImage | cut -f1))"
