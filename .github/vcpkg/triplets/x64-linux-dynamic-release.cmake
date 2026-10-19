# vcpkg's stock Linux triplets are either static (every GDAL tool would carry its own
# copy of GDAL) or build debug + release. This one is shared libraries, release only:
# the tools share one libgdal and the AppImage carries no debug builds.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_BUILD_TYPE release)
# GDAL's GRIB driver decodes CCSDS (template 5.42, ECMWF's IFS / AIFS open data) only when built against libaec: the vcpkg port does not switch it on
set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DGDAL_USE_LIBAEC=ON")
