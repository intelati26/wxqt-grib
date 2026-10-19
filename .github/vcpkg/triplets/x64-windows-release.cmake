# vcpkg's own x64-windows-release (shared libraries, release only) with one more switch: GDAL's GRIB
# driver decodes CCSDS (template 5.42, ECMWF's IFS / AIFS open data) only when built against libaec,
# and the vcpkg gdal port does not turn that on.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_BUILD_TYPE release)
set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DGDAL_USE_LIBAEC=ON")
