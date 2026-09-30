# package-windows.ps1
#
# Builds a self-contained "portable" folder for wxqt on Windows: copies
# wxqt.exe plus every Qt DLL/plugin it needs (via windeployqt, from the
# official Qt/MSVC install - MSYS2 does not package QtWebEngine at all, for
# any subsystem, so wxqt.exe itself must be an MSVC build) and the MSYS2-
# built GDAL command-line tools + their own dependency closure (GDAL tools
# run as separate subprocesses - QStandardPaths::findExecutable, not linked
# into wxqt.exe - so the MinGW/MSVC toolchain mismatch between them doesn't
# matter; they never share a process or link against each other), so the
# folder runs on a plain Windows install with none of that already present.
#
# Run from a shell with both the MSVC dev environment (ilammy/msvc-dev-cmd)
# and the official Qt bin dir on PATH (jurplel/install-qt-action), after a
# successful build (build/release/wxqt.exe must exist). Used by
# .github/workflows/build.yml's windows-x64 job.
$ErrorActionPreference = "Stop"

$exeSrc = "build/release/wxqt.exe"
if (-not (Test-Path $exeSrc)) {
    Write-Error "$exeSrc not found - build it first."
    exit 1
}

$distDir = "dist/wxqt-portable"
Write-Host "==> packaging into $distDir"
Remove-Item -Recurse -Force $distDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $distDir | Out-Null
Copy-Item $exeSrc "$distDir/wxqt.exe"

# ---------------------------------------------------------------------------
# Qt DLLs, plugins, and (since wxqt links QtWebEngineWidgets) the
# QtWebEngineProcess.exe + resources/ + translations/qtwebengine_locales
# Chromium needs, plus the VC++ runtime DLLs via --compiler-runtime.
# ---------------------------------------------------------------------------
Write-Host "==> running windeployqt"
# --no-translations / --no-opengl-sw / --no-system-d3d-compiler: this app does not
# use Qt's own translations, the software OpenGL fallback DLL (~20 MB) or the
# bundled D3D compiler, so leave them out of the download
windeployqt --release --compiler-runtime --no-translations --no-opengl-sw --no-system-d3d-compiler "$distDir/wxqt.exe"
if ($LASTEXITCODE -ne 0) {
    Write-Error "windeployqt failed"
    exit 1
}

# ---------------------------------------------------------------------------
# GDAL - MSYS2-built (mingw-w64-ucrt-x86_64-gdal), used by the RRFS GRIB/SPC
# Post/severe-indices/REFS viewers via QStandardPaths::findExecutable
# ("gdalwarp"), i.e. found through PATH at runtime, never linked into
# wxqt.exe. Copies the tools plus every /ucrt64/bin/*.dll they need
# (directly or transitively), repeating until nothing new turns up.
# ---------------------------------------------------------------------------
if (-not $env:MSYS2_LOCATION) {
    Write-Error "MSYS2_LOCATION env var not set - pass steps.msys2.outputs.msys2-location through as env in the workflow"
    exit 1
}
$gdalToolsBin = Join-Path $env:MSYS2_LOCATION "ucrt64/bin"
$gdalwarpPath = Join-Path $gdalToolsBin "gdalwarp.exe"

if (Test-Path $gdalwarpPath) {
    Write-Host "==> GDAL found at $gdalToolsBin - bundling tools + dependencies"
    $gdalDir = "$distDir/gdal"
    New-Item -ItemType Directory -Force -Path $gdalDir | Out-Null
    foreach ($tool in @("gdalwarp", "gdaldem", "gdal_translate", "gdal_rasterize", "gdalinfo", "gdal_contour", "ogr2ogr", "gdallocationinfo")) {
        $toolPath = Join-Path $gdalToolsBin "$tool.exe"
        if (Test-Path $toolPath) {
            Copy-Item $toolPath $gdalDir
        }
    }

    # MSYS2's own ldd (bash) is more reliable for walking the MinGW
    # dependency graph than trying to parse `dumpbin` output here - shell
    # out to the msys2 bash this job's MSYS2 setup step already installed.
    $msys2Bash = Join-Path $env:MSYS2_LOCATION "usr/bin/bash.exe"
    $sweepScript = @'
dir="$1"
added=1
while [ "$added" -eq 1 ]; do
    added=0
    while IFS= read -r bin; do
        for dep in $(ldd "$bin" 2>/dev/null | awk '{print $3}'); do
            case "$dep" in
                /ucrt64/bin/*)
                    name="$(basename "$dep")"
                    if [ ! -f "$dir/$name" ]; then
                        cp "$dep" "$dir/"
                        added=1
                    fi
                    ;;
            esac
        done
    done < <(find "$dir" -maxdepth 1 -type f \( -name '*.exe' -o -name '*.dll' \))
done
'@
    $sweepScriptPath = "$env:TEMP/sweep.sh"
    Set-Content -Path $sweepScriptPath -Value $sweepScript -NoNewline

    # MSYS2 bash needs /c/... style paths, not "C:\" ones - and needs an
    # absolute path regardless (this script's own cwd isn't necessarily
    # where MSYS2 bash's cwd would default to).
    function ToMsysPath($winPath) {
        $abs = (Resolve-Path $winPath).Path
        return "/" + $abs.Substring(0, 1).ToLower() + $abs.Substring(2).Replace("\", "/")
    }
    $sweepScriptUnix = ToMsysPath $sweepScriptPath
    $gdalDirUnix = ToMsysPath $gdalDir
    & $msys2Bash -lc "bash '$sweepScriptUnix' '$gdalDirUnix'"

    $gdalDataSrc = Join-Path (Split-Path $gdalToolsBin -Parent) "share/gdal"
    $projDataSrc = Join-Path (Split-Path $gdalToolsBin -Parent) "share/proj"
    if (Test-Path $gdalDataSrc) { Copy-Item -Recurse $gdalDataSrc "$gdalDir/gdal-data" }
    if (Test-Path $projDataSrc) { Copy-Item -Recurse $projDataSrc "$gdalDir/proj-data" }

    # No Python needed: rendering uses only the GDAL binaries copied above
    # (gdal_calc/gdal_merge were removed from every pipeline).
} else {
    Write-Host "==> note: GDAL not found - skipping. GRIB/SPC Post/severe-indices/REFS viewers will report 'GDAL not found'."
}

# ---------------------------------------------------------------------------
# launcher - puts the bundled gdal/ folder on PATH and points GDAL/PROJ at
# their bundled data dirs before starting wxqt.exe. Launch via this, not
# wxqt.exe directly, so the bundled GDAL tools (if present) are found.
# ---------------------------------------------------------------------------
@"
@echo off
setlocal
set "HERE=%~dp0"
if exist "%HERE%gdal" (
    set "PATH=%HERE%gdal;%PATH%"
    set "GDAL_DATA=%HERE%gdal\gdal-data"
    set "PROJ_DATA=%HERE%gdal\proj-data"
)
start "" "%HERE%wxqt.exe" %*
"@ | Set-Content -Path "$distDir/wxqt.bat"

$size = (Get-ChildItem -Recurse $distDir | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Host ("==> done: {0} ({1:N0} MB)" -f $distDir, $size)
