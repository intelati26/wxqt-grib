// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsModelFile.h"

// The built-in models that are written as definition files (GfsModelFile.h). The ones that are not (the GFS, the blend, the GEFS, the REFS, the hurricane model, SPC's REFS store) have
// file names or protocols this format does not describe and stay in GfsData.cpp.
const std::vector<std::pair<std::string, std::string>>& GfsModelFile::builtIn() {
    static const std::vector<std::pair<std::string, std::string>> list{
        {"AIGFS", R"MODEL(
# NOAA's AI global model (GraphCast based): the pressure levels and the surface are files of their own
id = AIGFS
label = NOAA/NCEP AIGFS 0.25 degree (an AI model; experimental)
source_label = NOAA/NCEP AIGFS 0.25 degree (an AI model)
kind = grib
url = https://noaa-nws-graphcastgfs-pds.s3.amazonaws.com/aigfs.{date}/{cycle}/model/atmos/grib2/aigfs.t{cycle}z.{file}.f{hour3}.grib2
file_default = pres
file = when level~" mb$" => pres
file = sfc
probe_file = pres
cycle_hours = 6
lag_hours = 4
probe_hour = 0
hours = 0-384/6
overlays = true
list = true
clone = true
clone_ids = precip_p06, precip_p12, precip_p24, precip_p36, precip_p48, precip_p60, precip_ptot, 1000_500_thick, 1000_850_thick, 850_700_thick, 850_temp_mslp_precip, 10m_wnd_precip, 10m_wnd_2m_temp, 200_wnd_ht, 250_wnd_ht, 300_wnd_ht, 500_rh_ht, 500_wnd_ht, 500_vort_ht, 700_rh_ht, 850_rh_ht, 850_temp_ht, 850_vort_ht, 850vor_500ht_200wd, 925_temp_ht
clone_specific_humidity = true
)MODEL"},
        {"IFS", R"MODEL(
# ECMWF's open data (CC BY 4.0), replicated on Google's storage: a JSON index beside each file
id = IFS
label = ECMWF IFS 0.25 degree (open data, CC BY 4.0)
kind = grib
url = https://storage.googleapis.com/ecmwf-open-data/{date}/{cycle}z/ifs/0p25/oper/{date}{cycle}0000-{hour}h-oper-fc.grib2
index_format = ecmwf
index_suffix = .index
index_replaces = .grib2
value_scale = APCP:1000, TCDC:100
cycle_hours = 6
lag_hours = 8
probe_hour = 0
hours = 0-144/3, 150-240/6
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, RH, UGRD, VGRD, VVEL, SPFH, PRMSL, DPT, GUST, TCDC, PWAT, CAPE, APCP, PRATE
clone_levels = 1000 mb, 925 mb, 850 mb, 700 mb, 600 mb, 500 mb, 400 mb, 300 mb, 250 mb, 200 mb, 150 mb, 100 mb, 50 mb
clone_pieces = false
)MODEL"},
        {"AIFS", R"MODEL(
# ECMWF's open data (CC BY 4.0), replicated on Google's storage: a JSON index beside each file
id = AIFS
label = ECMWF AIFS 0.25 degree (an AI model; open data, CC BY 4.0)
kind = grib
url = https://storage.googleapis.com/ecmwf-open-data/{date}/{cycle}z/aifs-single/0p25/oper/{date}{cycle}0000-{hour}h-oper-fc.grib2
index_format = ecmwf
index_suffix = .index
index_replaces = .grib2
value_scale = APCP:1000, TCDC:100
cycle_hours = 6
lag_hours = 8
probe_hour = 0
hours = 0-360/6
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, RH, UGRD, VGRD, VVEL, SPFH, PRMSL, DPT, GUST, TCDC, PWAT, CAPE, APCP, PRATE
clone_levels = 1000 mb, 925 mb, 850 mb, 700 mb, 600 mb, 500 mb, 400 mb, 300 mb, 250 mb, 200 mb, 150 mb, 100 mb, 50 mb
clone_pieces = false
)MODEL"},
        {"GEPS", R"MODEL(
# Canada's ensemble: one file per field and hour, the 21 members as its messages
id = GEPS
label = ECCC GEPS 21 member ensemble, 0.5 degree
kind = whole
url = https://dd.weather.gc.ca/{date}/WXO-DD/ensemble/geps/grib2/raw/{cycle}/{hour3}/CMC_geps-raw_{file}_latlon0p5x0p5_{date}{cycle}_P{hour3}_allmbrs.grib2
file = when level~"^[0-9]+ mb$" => {variable}_ISBL_{mb4}
file = when variable=SPFH level="2 m above ground" => {variable}_TGL_2
file = when level="2 m above ground" => {variable}_TGL_2m
file = when variable=WIND level="10 m above ground" => {variable}_TGL_10
file = when level="10 m above ground" => {variable}_TGL_10m
file = when level="mean sea level" => {variable}_MSL_0
file = when level~"^entire atmosphere" => {variable}_EATM_0
file = {variable}_SFC_0
member = c00:1, p01-p20:2-21
probe_file = TMP_TGL_2m
cycle_hours = 12
lag_hours = 6
probe_hour = 384
cycles_to_try = 4
hours = 0-192/3, 198-384/6
run_every = 12
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, RH, UGRD, VGRD, VVEL, PRMSL, PWAT, CAPE, CIN, APCP, TCDC, SNOD, WEASD, TMAX, TMIN
clone_fields = HGT|10 mb, TMP|10 mb, UGRD|10 mb, VGRD|10 mb, HGT|50 mb, TMP|50 mb, UGRD|50 mb, VGRD|50 mb, HGT|100 mb, TMP|100 mb, UGRD|100 mb, VGRD|100 mb, HGT|200 mb, TMP|200 mb, UGRD|200 mb, VGRD|200 mb, HGT|250 mb, TMP|250 mb, UGRD|250 mb, VGRD|250 mb, HGT|500 mb, TMP|500 mb, UGRD|500 mb, VGRD|500 mb, HGT|700 mb, TMP|700 mb, UGRD|700 mb, VGRD|700 mb, HGT|850 mb, TMP|850 mb, UGRD|850 mb, VGRD|850 mb, HGT|925 mb, TMP|925 mb, UGRD|925 mb, VGRD|925 mb, HGT|1000 mb, TMP|1000 mb, UGRD|1000 mb, VGRD|1000 mb, RH|10 mb, RH|50 mb, RH|100 mb, RH|200 mb, RH|250 mb, RH|500 mb, RH|700 mb, RH|850 mb, RH|925 mb, RH|1000 mb, HGT|300 mb, UGRD|300 mb, VGRD|300 mb, HGT|400 mb, UGRD|400 mb, VGRD|400 mb, VVEL|850 mb
clone_pieces = false
clone_prefix = "Mean "
clone_skip = anom, 500_vort_ht, 850_vort_ht, 850_stream_vort, 850vor_500ht_200wd, precip_p01
)MODEL"},
        {"RRFS", R"MODEL(
# the 3 km Rapid Refresh Forecast System: the 2D fields and the pressure levels are files of their own
id = RRFS
label = NOAA/NCEP RRFS 3 km
kind = grib
url = https://noaa-rrfs-ops-pds.s3.amazonaws.com/rrfs.{date}/{cycle}/rrfs.t{cycle}z.{file}.f{hour3}.conus.grib2
file_default = 2dfld.3km
file = when level~"^[0-9]+ mb$" => prslev.3km
file = 2dfld.3km
probe_file = 2dfld.3km
cycle_hours = 1
lag_hours = 2
probe_hour = 0
cycles_to_try = 8
warp = 0.03, -127.0, 22.0, -65.0, 52.0
sectors = conus
hours = 0-84/1
hourly_runs = true
on_missing = previous_run
preload_hours = 12
run_length = every 6 : 84 , else : 18
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, RH, UGRD, VGRD, ABSV, DPT, PRMSL, PWAT, CAPE, CIN, APCP, CRAIN, CSNOW, CFRZR, CICEP, TCDC, GUST, SNOD, WEASD, REFC, HLCY, VIS, PRATE
clone_levels = 200 mb, 250 mb, 300 mb, 400 mb, 500 mb, 700 mb, 850 mb, 925 mb, 1000 mb
clone_skip = anom
clone_pieces = false
clone_rename = PRMSL:MSLET
)MODEL"},
        {"HREF", R"MODEL(
# the High-Resolution Ensemble Forecast on NOMADS: its own ready-made products (mean, pmmn, lpmm, avrg, sprd, prob, eas, ffri)
id = HREF
label = NOAA/NCEP HREF, 10 member 3 km ensemble
kind = grib
url = https://nomads.ncep.noaa.gov/pub/data/nccf/com/href/prod/href.{date}/ensprod/href.t{cycle}z.conus.{file}.f{hour2}.grib2
file_default = mean
file = when stat= => mean
file = {stat}
default_detail = *
probe_file = mean
cycle_hours = 6
lag_hours = 4
probe_hour = 36
cycles_to_try = 5
max_parallel = 4
warp = 0.03, -127.0, 22.0, -65.0, 52.0
sectors = conus
hours = 1-48/1
list = true
)MODEL"},
        {"SPCPOST", R"MODEL(
# SPC's post-processing of the REFS (SPC_POST v3, NWS SCN 26-77; the test area until it is in the real one)
id = SPCPOST
label = SPC post-processing of the REFS: calibrated thunder and severe guidance
source_label = SPC post-processing of the REFS
kind = whole
url = https://nomads.ncep.noaa.gov/pub/data/nccf/com/spc_post/prod/spc_post.{date}/{dir}/spc_post.t{cycle}z.{name}.f{hour3}.grib2
url = https://nomads.ncep.noaa.gov/pub/data/nccf/com/spc_post/para/spc_post.{date}/{dir}/spc_post.t{cycle}z.{name}.f{hour3}.grib2
file = {level}
member = b1-b9:1-9
probe_file = refsct/refsct_1hr
probe_hour = 1
cycle_hours = 6
lag_hours = 5
cycles_to_try = 4
max_parallel = 4
warp = 0.1, -170.0, 10.0, -50.0, 72.0
sectors = conus
hours = 1-60/1
run_every = 6
list = true
)MODEL"},
        {"SPCGEFS", R"MODEL(
# SPC's post-processing of the GEFS: the daily calibrated thunder and the machine-learning severe guidance
id = SPCGEFS
label = SPC post-processing of the GEFS: daily calibrated thunder, machine-learning severe
source_label = SPC post-processing of the GEFS
kind = whole
url = https://nomads.ncep.noaa.gov/pub/data/nccf/com/spc_post/prod/spc_post.{date}/{dir}/spc_post.t{cycle}z.{name}.f{hour3}.grib2
url = https://nomads.ncep.noaa.gov/pub/data/nccf/com/spc_post/para/spc_post.{date}/{dir}/spc_post.t{cycle}z.{name}.f{hour3}.grib2
file = {level}
member = b1-b9:1-9
probe_file = gefsct/gefsct
probe_hour = 36
cycle_hours = 12
lag_hours = 6
cycles_to_try = 4
max_parallel = 4
warp = 0.1, -170.0, 10.0, -50.0, 72.0
sectors = conus
hours = 36-204/24
run_every = 12
list = true
)MODEL"},
        {"SFCOA", R"MODEL(
# SPC's mesoanalysis (surface objective analysis, HRRR background), one NetCDF file an hour: a field is read by range
id = SFCOA
label = SPC mesoanalysis (SFCOA-HRRR)
kind = netcdf
url = https://nomads.ncep.noaa.gov/pub/data/nccf/com/spc_post/prod/spc_post.{date}/sfcoa-hrrr/spc_post.t{cycle}z.sfcoa_hrrr.final.nc
url = https://nomads.ncep.noaa.gov/pub/data/nccf/com/spc_post/para/spc_post.{date}/sfcoa-hrrr/spc_post.t{cycle}z.sfcoa_hrrr.final.nc
probe_hour = 0
cycle_hours = 1
lag_hours = 1
cycles_to_try = 6
max_parallel = 4
warp = 0.03, -127.0, 22.0, -65.0, 52.0
sectors = conus
hours = 0-0/1
hourly_runs = true
list = true
)MODEL"},
        {"RRFSD", R"MODEL(
# the same fields computed from the RRFS forecasts, a NetCDF file for each hour of each run
id = RRFSD
label = SPC severe weather fields from the RRFS (RRFSD)
source_label = SPC severe weather fields from the RRFS
kind = netcdf
url = https://nomads.ncep.noaa.gov/pub/data/nccf/com/spc_post/prod/spc_post.{date}/rrfsd/spc_post.t{cycle}z.rrfs_d.f{hour3}.nc
url = https://nomads.ncep.noaa.gov/pub/data/nccf/com/spc_post/para/spc_post.{date}/rrfsd/spc_post.t{cycle}z.rrfs_d.f{hour3}.nc
probe_hour = 1
cycle_hours = 1
lag_hours = 3
cycles_to_try = 8
max_parallel = 4
warp = 0.03, -127.0, 22.0, -65.0, 52.0
sectors = conus
hours = 0-84/1
hourly_runs = true
on_missing = previous_run
preload_hours = 6
run_length = every 6 : 84 , else : 18
list = true
)MODEL"},
        {"GFS-WAVE", R"MODEL(
# GFS-Wave: the global 0.16 degree sea state, one file for each hour
id = GFS-WAVE
label = NOAA/NCEP GFS-Wave 0.16 degree
kind = grib
url = https://noaa-gfs-bdp-pds.s3.amazonaws.com/gfs.{date}/{cycle}/wave/gridded/gfswave.t{cycle}z.global.0p16.f{hour3}.grib2
cycle_hours = 6
lag_hours = 5
probe_hour = 384
extra_missing = 9999
hours = 0-120/1, 123-384/3
list = true
)MODEL"},
        {"GEFS-WAVE", R"MODEL(
# GEFS-Wave: the control member's sea state, 0.25 degree
id = GEFS-WAVE
label = NOAA/NCEP GEFS-Wave, control member, 0.25 degree
kind = grib
url = https://noaa-gefs-pds.s3.amazonaws.com/gefs.{date}/{cycle}/wave/gridded/gefs.wave.t{cycle}z.c00.global.0p25.f{hour3}.grib2
cycle_hours = 6
lag_hours = 7
probe_hour = 384
extra_missing = 9999
hours = 0-240/3, 246-384/6
list = true
)MODEL"},
    };
    return list;
}
