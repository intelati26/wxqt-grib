# package-windows.ps1
#
# Builds a self-contained "portable" folder for wxqt on Windows: copies
# wxqt.exe plus every Qt DLL/plugin it needs (via windeployqt, from the
# official Qt/MSVC install) and the minimal vcpkg-built GDAL command-line
# tools + their DLLs (GDAL tools run as separate subprocesses -
# QStandardPaths::findExecutable, not linked into wxqt.exe), so the folder
# runs on a plain Windows install with none of that already present.
#
# Run from a shell with both the MSVC dev environment and the official Qt bin
# dir on PATH (jurplel/install-qt-action), after a successful build
# (build/release/wxqt.exe must exist), with GDAL_PREFIX pointing at the vcpkg
# GDAL install (optional locally: without it GDAL is left out). Used by
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

# --compiler-runtime drops the 18 MB vc_redist.x64.exe installer in the folder,
# which a portable zip cannot run anyway. Ship the handful of runtime DLLs it
# would install instead (app-local deployment of the VC++ runtime is supported);
# the Universal CRT is part of Windows 10/11 itself. VCToolsRedistDir is set by
# the "Set up MSVC environment" step.
Remove-Item "$distDir/vc_redist*.exe" -ErrorAction SilentlyContinue
$crtDir = Get-ChildItem "$env:VCToolsRedistDir/x64/Microsoft.VC*.CRT" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $crtDir) {
    Write-Error "VC++ runtime DLLs not found under VCToolsRedistDir ($env:VCToolsRedistDir) - refusing to ship a package that needs the redistributable installed"
    exit 1
}
Copy-Item "$($crtDir.FullName)/*.dll" $distDir
Write-Host "==> VC++ runtime DLLs copied from $($crtDir.FullName)"

# Direct3D shader compiler DLLs (~14 MB) are only for Qt's D3D12 rendering backend,
# not the software/raster path this widget app uses.
Remove-Item "$distDir/dxcompiler.dll", "$distDir/dxil.dll" -ErrorAction SilentlyContinue

# ---------------------------------------------------------------------------
# GDAL - a minimal vcpkg build (.github/vcpkg/vcpkg.json: only the drivers
# wxqt uses), used by the RRFS GRIB/SPC Post/severe-indices/REFS viewers via
# QStandardPaths::findExecutable("gdalwarp"), i.e. found through PATH at
# runtime, never linked into wxqt.exe. vcpkg already puts each tool's DLLs
# next to it in tools/gdal; GDAL_PREFIX is the vcpkg install dir for the
# x64-windows-release triplet.
# ---------------------------------------------------------------------------
$gdalToolsBin = if ($env:GDAL_PREFIX) { Join-Path $env:GDAL_PREFIX "tools/gdal" } else { "" }
$gdalwarpPath = if ($gdalToolsBin) { Join-Path $gdalToolsBin "gdalwarp.exe" } else { "" }
if ($env:GDAL_PREFIX -and -not (Test-Path $gdalwarpPath)) {
    Write-Error "GDAL_PREFIX is set but $gdalwarpPath does not exist - refusing to ship a package without GDAL"
    exit 1
}

if ($gdalwarpPath) {
    Write-Host "==> GDAL found at $gdalToolsBin - bundling tools + dependencies"
    $gdalDir = "$distDir/gdal"
    New-Item -ItemType Directory -Force -Path $gdalDir | Out-Null
    foreach ($tool in @("gdalwarp", "gdaldem", "gdal_translate", "gdal_rasterize", "gdalinfo", "gdal_contour", "ogr2ogr", "gdallocationinfo", "ogrinfo")) {
        Copy-Item (Join-Path $gdalToolsBin "$tool.exe") $gdalDir
    }
    Copy-Item "$gdalToolsBin/*.dll" $gdalDir
    # MSVC-built like wxqt.exe, so they need the same VC++ runtime DLLs - next
    # to them, since a child process doesn't search its parent's folder
    Copy-Item "$($crtDir.FullName)/*.dll" $gdalDir

    # data files only - share/gdal and share/proj also hold vcpkg's CMake
    # package files, copyright and usage notes
    $skipData = @("*.cmake", "vcpkg*", "copyright", "usage")
    foreach ($pair in @(@("gdal", "gdal-data"), @("proj", "proj-data"))) {
        $src = Join-Path $env:GDAL_PREFIX "share/$($pair[0])"
        $dst = Join-Path $gdalDir $pair[1]
        New-Item -ItemType Directory -Force -Path $dst | Out-Null
        Get-ChildItem $src -File | Where-Object { $name = $_.Name; -not ($skipData | Where-Object { $name -like $_ }) } |
            Copy-Item -Destination $dst
    }
    # gdal-data is hundreds of small tables; ship it as ONE archive that GDAL reads in place
    # (GDAL_DATA=/vsizip/<zip>) instead of as loose files. proj-data stays a folder: proj.db
    # is a SQLite database and cannot be read from inside a zip.
    $gdalDataDir = Join-Path $gdalDir "gdal-data"
    $gdalDataZip = Join-Path $gdalDir "gdal-data.zip"
    Remove-Item $gdalDataZip -ErrorAction SilentlyContinue
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::CreateFromDirectory($gdalDataDir, $gdalDataZip, [System.IO.Compression.CompressionLevel]::Optimal, $false)
    Remove-Item $gdalDataDir -Recurse -Force
    Write-Host ("==> gdal-data archived: {0:N2} MB" -f ((Get-Item $gdalDataZip).Length / 1MB))
    if (-not (Test-Path "$gdalDir/proj-data/proj.db")) {
        Write-Error "proj.db not found under $env:GDAL_PREFIX/share/proj"
        exit 1
    }

    # No Python needed: rendering uses only the GDAL binaries copied above
    # (gdal_calc/gdal_merge were removed from every pipeline).
} else {
    Write-Host "==> note: GDAL not found - skipping. GRIB/SPC Post/severe-indices/REFS viewers will report 'GDAL not found'."
}

# ---------------------------------------------------------------------------
# img2webp (vcpkg libwebp) for the Save dialog's WebP export, in the tools/
# folder next to wxqt.exe - where UtilityTools looks first. vcpkg puts its
# DLLs next to it; MSVC-built, so it gets the VC++ runtime DLLs too.
# WEBP_PREFIX is the vcpkg install dir for the x64-windows-release triplet.
# ---------------------------------------------------------------------------
if ($env:WEBP_PREFIX) {
    $webpBin = Join-Path $env:WEBP_PREFIX "tools/libwebp"
    if (-not (Test-Path "$webpBin/img2webp.exe")) {
        Write-Error "WEBP_PREFIX is set but $webpBin/img2webp.exe does not exist - refusing to ship without it"
        exit 1
    }
    $toolsDir = "$distDir/tools"
    New-Item -ItemType Directory -Force -Path $toolsDir | Out-Null
    Copy-Item "$webpBin/img2webp.exe" $toolsDir
    Copy-Item "$webpBin/*.dll" $toolsDir
    Copy-Item "$($crtDir.FullName)/*.dll" $toolsDir
    Write-Host "==> img2webp bundled into $toolsDir"
} else {
    Write-Host "==> note: WEBP_PREFIX not set - WebP export will need img2webp or ffmpeg installed."
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
    set "GDAL_DATA=/vsizip/%HERE:\=/%gdal/gdal-data.zip"
    set "PROJ_DATA=%HERE%gdal\proj-data"
)
start "" "%HERE%wxqt.exe" %*
"@ | Set-Content -Path "$distDir/wxqt.bat"

$size = (Get-ChildItem -Recurse $distDir | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Host ("==> done: {0} ({1:N0} MB)" -f $distDir, $size)
