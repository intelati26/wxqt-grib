# vcpkg's stock Linux triplets are either static (every GDAL tool would carry its own
# copy of GDAL) or build debug + release. This one is shared libraries, release only:
# the tools share one libgdal and the AppImage carries no debug builds.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_BUILD_TYPE release)
