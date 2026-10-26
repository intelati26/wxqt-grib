// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsModelFile.h"

// The built-in models that are written as definition files (GfsModelFile.h). The ones that are not (the GFS, the blend, the GEFS, the REFS, the hurricane model, SPC's REFS store) have
// file names or protocols this format does not describe and stay in GfsData.cpp.
const std::vector<std::pair<std::string, std::string>>& GfsModelFile::builtIn() {
    static const std::vector<std::pair<std::string, std::string>> list{
        {"GEOS-FP", R"MODEL(
# NASA's GEOS-FP (Goddard Earth Observing System, forecast): the global 0.25 x 0.3125 degree system of the Global Modeling and Assimilation Office. The files are NetCDF-4, one per hour and
# collection; this reads the hourly time-averaged single-level collection (tavg1_2d_slv_Nx), which has the 2 m and 10 m fields, the sea level pressure and the heights, temperatures and winds
# at 1000, 850, 500 and 250 mb. The 3 hourly pressure-level collection (inst3_3d_asm_Np, 0.5 GB a file) would have every level. The 00Z run goes 10 days, the 12Z 5 days, the others 30 hours.
id = GEOS-FP
label = NASA GMAO GEOS-FP 0.25 degree
kind = netcdf
# the directory is the run; the file name has the run, then the time the average is for (stamped half an hour after the hour it begins); `+` is %2B in a url
url = https://portal.nccs.nasa.gov/datashare/gmao/geos-fp/forecast/Y{year}/M{month}/D{day}/H{cycle}/GEOS.fp.fcst.{file}.{date}_{cycle}%2B{valid}.V01.nc4
file = when level="aerosol" => tavg3_2d_aer_Nx
file = tavg1_2d_slv_Nx
valid_offset = 30
# the 3 hour aerosol averages are stamped in the middle of their period: the one that ends at forecast hour 3 of the 12Z run is 13:30
valid_offset_file = tavg3_2d_aer_Nx:-90
# the NOAA names the charts use -> this file's names
nc_variable = when variable=HGT level~"^[0-9]+ mb$" => H{mb}
nc_variable = when variable=TMP level~"^[0-9]+ mb$" => T{mb}
nc_variable = when variable=UGRD level~"^[0-9]+ mb$" => U{mb}
nc_variable = when variable=VGRD level~"^[0-9]+ mb$" => V{mb}
nc_variable = when variable=SPFH level~"^[0-9]+ mb$" => Q{mb}
nc_variable = when variable=VVEL level~"^[0-9]+ mb$" => OMEGA{mb}
nc_variable = when variable=TMP level="2 m above ground" => T2M
nc_variable = when variable=SPFH level="2 m above ground" => QV2M
nc_variable = when variable=UGRD level="10 m above ground" => U10M
nc_variable = when variable=VGRD level="10 m above ground" => V10M
nc_variable = when variable=PRMSL => SLP
nc_variable = when variable=PRES level=surface => PS
nc_variable = when variable=PWAT => TQV
# Kelvin to degrees C, as the GRIB models arrive
value_offset = TMP:-273.15
# the grid is 0.3125 degrees of longitude by 0.25 of latitude: warped to a square 0.25 degree grid
warp = 0.25, -180.0, -90.0, 180.0, 90.0
probe_file = tavg1_2d_slv_Nx
probe_hour = 0
cycle_hours = 6
lag_hours = 8
cycles_to_try = 4
max_parallel = 4
hours = 0-239/1
run_length = 0:240, 12:120, else:30
run_every = 6
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, UGRD, VGRD, SPFH, VVEL, PRMSL, PWAT, PRES, RH
clone_fields = HGT|1000 mb, HGT|850 mb, HGT|500 mb, HGT|250 mb, TMP|850 mb, TMP|500 mb, TMP|250 mb, UGRD|850 mb, UGRD|500 mb, UGRD|250 mb, VGRD|850 mb, VGRD|500 mb, VGRD|250 mb, SPFH|850 mb, SPFH|500 mb, SPFH|250 mb, VVEL|500 mb, RH|850 mb, RH|500 mb, RH|250 mb
clone_pieces = false
clone_specific_humidity = true
clone_skip = anom, precip, thick, 850_vort_ht, 500_vort_ht
)MODEL"},
        {"GFS", R"MODEL(
# the GFS: the charts everything else is made from; NOMADS can cut a box of the fields out of a file, which is what a regional chart fetches
id = GFS
label = NOAA/NCEP GFS 0.25 degree
kind = grib
url = https://noaa-gfs-bdp-pds.s3.amazonaws.com/gfs.{date}/{cycle}/atmos/gfs.t{cycle}z.pgrb2.0p25.f{hour3}
subset_url = https://nomads.ncep.noaa.gov/cgi-bin/filter_gfs_0p25.pl?dir=%2Fgfs.{date}%2F{cycle}%2Fatmos&file=gfs.t{cycle}z.pgrb2.0p25.f{hour3}{variables}{levels}&subregion=&leftlon={west}&rightlon={east}&toplat={north}&bottomlat={south}
cycle_hours = 6
lag_hours = 3
probe_hour = 0
hours = 0-243/3, 252-396/12
overlays = true
)MODEL"},
        {"NBM", R"MODEL(
# the National Blend of Models: a run every hour (no hour 0 file), a Lambert grid warped to latitude / longitude, only the contiguous United States
id = NBM
label = NOAA/NWS National Blend of Models v4, 2.5 km
kind = grib
url = https://noaa-nbm-grib2-pds.s3.amazonaws.com/blend.{date}/{cycle}/core/blend.t{cycle}z.core.f{hour3}.co.grib2
cycle_hours = 1
lag_hours = 1
probe_hour = 1
cycles_to_try = 8
warp = 0.025, -127.0, 22.0, -65.0, 52.0
sectors = conus
hours = 1-36/1, 39-192/3, 198-264/6
hourly_runs = true
on_missing = main_run
)MODEL"},
        {"GEFS", R"MODEL(
# the GEFS: the mean ("avg"), spread ("spr") and members (c00, p01 ... p30) are files of their own, in three sets: the 0.5 degree "a" set, the 0.25 degree surface set "s" (gusts, dew point, visibility,
# helicity, surface-based CAPE and CIN) and the second 0.5 degree set "b" (the most unstable parcel's CAPE). A file here is "<avg|spr|member>-<a|s|b>"; the name is "ge" and its first three letters.
id = GEFS
label = NOAA/NCEP GEFS 30 member ensemble, 0.5 degree
source_label = NOAA/NCEP GEFS mean and spread, 0.5 degree
kind = grib
url = when file~"-s$" => https://noaa-gefs-pds.s3.amazonaws.com/gefs.{date}/{cycle}/atmos/pgrb2sp25/ge{file3}.t{cycle}z.pgrb2s.0p25.f{hour3}
url = when file~"-b$" => https://noaa-gefs-pds.s3.amazonaws.com/gefs.{date}/{cycle}/atmos/pgrb2bp5/ge{file3}.t{cycle}z.pgrb2b.0p50.f{hour3}
url = https://noaa-gefs-pds.s3.amazonaws.com/gefs.{date}/{cycle}/atmos/pgrb2ap5/ge{file3}.t{cycle}z.pgrb2a.0p50.f{hour3}
file_default = avg-a
file = when variable=GUST,DPT,VIS,HLCY,MSLET stat~"^[cp][0-9][0-9]$" => {stat}-s
file = when variable=GUST,DPT,VIS,HLCY,MSLET stat=spr => spr-s
file = when variable=GUST,DPT,VIS,HLCY,MSLET => avg-s
file = when variable=CAPE,CIN level=surface stat~"^[cp][0-9][0-9]$" => {stat}-s
file = when variable=CAPE,CIN level=surface stat=spr => spr-s
file = when variable=CAPE,CIN level=surface => avg-s
file = when variable=CAPE level~"^255" stat~"^[cp][0-9][0-9]$" => {stat}-b
file = when variable=CAPE level~"^255" stat=spr => spr-b
file = when variable=CAPE level~"^255" => avg-b
file = when stat~"^[cp][0-9][0-9]$" => {stat}-a
file = when stat=spr => spr-a
file = avg-a
default_detail = *
probe_file = avg-a
cycle_hours = 6
lag_hours = 6
probe_hour = 384
hours = 0-384/6
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, RH, UGRD, VGRD, VVEL, PRMSL, PWAT, CAPE, CIN, APCP, CRAIN, CSNOW, CFRZR, CICEP, TCDC, DPT, GUST, SNOD, WEASD, TMAX, TMIN, HLCY
clone_levels = 10 mb, 50 mb, 100 mb, 200 mb, 250 mb, 300 mb, 400 mb, 500 mb, 700 mb, 850 mb, 925 mb, 1000 mb
clone_pieces = true
clone_prefix = "Mean "
clone_plain = precip_p
clone_skip = precip_p06, precip_p24, 500_vort_ht, 850_vort_ht
)MODEL"},
        {"REFS", R"MODEL(
# the RRFS ensemble: each member's files (rrfsens), the ready-made products (refs ensprod: "ens:avrg", "ens:eas", "ens:ffri") and the deterministic RRFS run of the same cycle (rrfs/...) for the charts
# that put it with the members. A member's file is "<m001>/<2dfld|prslev>".
id = REFS
label = NOAA/NCEP REFS, 5 member 3 km ensemble
source_label = NOAA/NCEP REFS
kind = grib
url = when file~"^rrfs/" => https://noaa-rrfs-ops-pds.s3.amazonaws.com/rrfs.{date}/{cycle}/rrfs.t{cycle}z.{name}.f{hour3}.conus.grib2
url = when file~"^ens:" => https://noaa-rrfs-ops-pds.s3.amazonaws.com/refs.{date}/{cycle}/ensprod/refs.t{cycle}z.{tail}.f{hour2}.conus.grib2
url = https://noaa-rrfs-ops-pds.s3.amazonaws.com/rrfsens.{date}/{cycle}/{dir}/rrfs.t{cycle}z.{dir}.{name}nomads.3km.f{hour3}.conus.grib2
file = when stat~"^ens:" => {stat}
file = when stat=rrfs level~"^[0-9]+ mb$" => rrfs/prslev.3km
file = when stat=rrfs => rrfs/2dfld.3km
file = when stat= level~"^[0-9]+ mb$" => m001/prslev
file = when stat= => m001/2dfld
file = when level~"^[0-9]+ mb$" => {stat}/prslev
file = {stat}/2dfld
default_detail = *
probe_file = m001/2dfld
cycle_hours = 6
lag_hours = 5
probe_hour = 60
cycles_to_try = 4
warp = 0.03, -127.0, 22.0, -65.0, 52.0
sectors = conus
hours = 0-60/1
list = true
)MODEL"},
        {"HAFSA", R"MODEL(
# NCEP's hurricane model, version A, for one storm at a time ({storm} is the NHC number and basin letter, "09l"): a storm-following grid of 0.02 degrees (the atmosphere and the simulated
# satellite are files of their own, 3 hourly to 126 hours), the parent domain's swath, the track, and one file of waves for all the hours (the hour is in the record, not the file name)
id = HAFSA
label = NOAA/NCEP HAFS-A, 2 km storm-following grid
source_label = NOAA/NCEP HAFS-A storm-following grid, 2 km
kind = grib
storm = true
url = when file=swath => https://nomads.ncep.noaa.gov/pub/data/nccf/com/hafs/prod/hfsa.{date}/{cycle}/{storm}.{date}{cycle}.hfsa.parent.swath.grb2
url = when file=trak => https://nomads.ncep.noaa.gov/pub/data/nccf/com/hafs/prod/hfsa.{date}/{cycle}/{storm}.{date}{cycle}.hfsa.trak.atcfunix
url = when file=ww3 => https://nomads.ncep.noaa.gov/pub/data/nccf/com/hafs/prod/hfsa.{date}/{cycle}/{storm}.{date}{cycle}.hfsa.ww3.grb2
url = https://nomads.ncep.noaa.gov/pub/data/nccf/com/hafs/prod/hfsa.{date}/{cycle}/{storm}.{date}{cycle}.hfsa.{file}.f{hour3}.grb2
file_default = storm.atm
file = when stat=swath => swath
file = when stat=ww3 => ww3
file = when variable~"^var" => storm.sat
file = storm.atm
default_detail = *
extra_missing = 9999
probe_file = storm.atm
cycle_hours = 6
lag_hours = 4
probe_hour = 126
max_parallel = 4
cycles_to_try = 4
sectors = storm
hours = 0-126/3
on_missing = previous_cycle
list = true
)MODEL"},
        {"HAFSB", R"MODEL(
# NCEP's hurricane model, version B, for one storm at a time ({storm} is the NHC number and basin letter, "09l"): a storm-following grid of 0.02 degrees (the atmosphere and the simulated
# satellite are files of their own, 3 hourly to 126 hours), the parent domain's swath, the track, and one file of waves for all the hours (the hour is in the record, not the file name)
id = HAFSB
label = NOAA/NCEP HAFS-B, 2 km storm-following grid
source_label = NOAA/NCEP HAFS-B storm-following grid, 2 km
kind = grib
storm = true
url = when file=swath => https://nomads.ncep.noaa.gov/pub/data/nccf/com/hafs/prod/hfsb.{date}/{cycle}/{storm}.{date}{cycle}.hfsb.parent.swath.grb2
url = when file=trak => https://nomads.ncep.noaa.gov/pub/data/nccf/com/hafs/prod/hfsb.{date}/{cycle}/{storm}.{date}{cycle}.hfsb.trak.atcfunix
url = when file=ww3 => https://nomads.ncep.noaa.gov/pub/data/nccf/com/hafs/prod/hfsb.{date}/{cycle}/{storm}.{date}{cycle}.hfsb.ww3.grb2
url = https://nomads.ncep.noaa.gov/pub/data/nccf/com/hafs/prod/hfsb.{date}/{cycle}/{storm}.{date}{cycle}.hfsb.{file}.f{hour3}.grb2
file_default = storm.atm
file = when stat=swath => swath
file = when stat=ww3 => ww3
file = when variable~"^var" => storm.sat
file = storm.atm
default_detail = *
extra_missing = 9999
probe_file = storm.atm
cycle_hours = 6
lag_hours = 4
probe_hour = 126
max_parallel = 4
cycles_to_try = 4
sectors = storm
hours = 0-126/3
on_missing = previous_cycle
list = true
)MODEL"},
        {"SPCREFS", R"MODEL(
# SPC's REFS ensemble products, an open Zarr store a cycle: read by the program's own reader (the loader registered for this id), not by a download; the file is the store's own path
id = SPCREFS
label = SPC REFS ensemble products (open Zarr data)
kind = custom
url = https://www.spc.noaa.gov/exper/refs/data/{year}/{month}/{day}/refs-spc_{date}_{cycle}00.zarr{/file}
probe_file = .zgroup
probe_hour = 0
cycle_hours = 6
lag_hours = 3
cycles_to_try = 6
max_parallel = 4
warp = 0.03, -127.0, 22.0, -65.0, 52.0
sectors = conus
hours = 1-60/1
run_every = 6
list = true
)MODEL"},
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
clone_skip = precip_p01
)MODEL"},
        {"IFS-ENS", R"MODEL(
# ECMWF's ensemble (ENS): 50 members of the physical model, each a message of the step's file (4 GB a step, read by its JSON index); a member is chosen by its number (`stat` e01 ... e50)
id = IFS-ENS
label = ECMWF IFS ensemble (ENS), 50 members, 0.25 degree (open data, CC BY 4.0)
kind = grib
url = https://storage.googleapis.com/ecmwf-open-data/{date}/{cycle}z/ifs/0p25/enfo/{date}{cycle}0000-{hour}h-enfo-ef.grib2
index_format = ecmwf
index_suffix = .index
index_replaces = .grib2
value_scale = APCP:1000
cycle_hours = 6
lag_hours = 9
probe_hour = 0
max_parallel = 6
hours = 0-144/3, 150-360/6
run_length = 0:360, 12:360, else:144
run_every = 6
list = true
)MODEL"},
        {"ENS-ALL", R"MODEL(
# the ECMWF ensemble, the GEFS and the GEPS read together as one ensemble: its run is the ECMWF's (the 00Z and 12Z are the runs all three make), the members of the others are read by the
# charts for the same valid time (GfsRender, pool); this file is only the ECMWF's index and the hours the three share
id = ENS-ALL
label = All ensembles pooled: ECMWF ENS, GEFS and GEPS (102 members), 0.5 degree
kind = grib
url = https://storage.googleapis.com/ecmwf-open-data/{date}/{cycle}z/ifs/0p25/enfo/{date}{cycle}0000-{hour}h-enfo-ef.grib2
index_format = ecmwf
index_suffix = .index
index_replaces = .grib2
cycle_hours = 12
lag_hours = 10
probe_hour = 0
max_parallel = 6
hours = 6-240/6
run_length = 0:240, 12:240, else:240
run_every = 12
list = true
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
clone_variables = HGT, TMP, RH, UGRD, VGRD, VVEL, SPFH, PRMSL, DPT, GUST, TCDC, APCP, PRATE
# the open data has specific humidity on the pressure levels, not relative: the charts of relative humidity are worked out from it, with the temperature
clone_specific_humidity = true
clone_levels = 1000 mb, 925 mb, 850 mb, 700 mb, 600 mb, 500 mb, 400 mb, 300 mb, 250 mb, 200 mb, 150 mb, 100 mb, 50 mb
clone_pieces = false
clone_skip = precip_p01, precip_p03, 1000_500_thick, 1000_850_thick, 850_700_thick, 850_temp_mslp_precip, 10m_wnd_precip
)MODEL"},
        {"HRDPS", R"MODEL(
# Canada's High Resolution Deterministic Prediction System: 2.5 km over Canada and the northern United States (a rotated latitude / longitude grid, put on a regular 0.05 degree one), every
# 6 hours to 48 hours, from Environment and Climate Change Canada's Datamart. A file for each field and hour (3 to 4 MB); only the last day or two are kept there.
id = HRDPS
label = ECCC HRDPS 2.5 km (Canada)
kind = whole
url = https://dd.weather.gc.ca/{date}/WXO-DD/model_hrdps/continental/2.5km/{cycle}/{hour3}/{date}T{cycle}Z_MSC_HRDPS_{file}_RLatLon0.0225_PT{hour3}H.grib2
file = when level~"^[0-9]+ mb$" => {variable}_ISBL_{mb4}
file = when variable=GUST => GUST_AGL-10m
file = when level="2 m above ground" => {variable}_AGL-2m
file = when level="10 m above ground" => {variable}_AGL-10m
file = when level="mean sea level" => PRMSL_MSL
file = {variable}_Sfc
probe_file = TMP_AGL-2m
probe_hour = 0
cycle_hours = 6
lag_hours = 5
cycles_to_try = 4
max_parallel = 4
warp = 0.05, -165.0, 20.0, -45.0, 78.0
sectors = world
hours = 0-48/1
run_every = 6
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, RH, UGRD, VGRD, DPT, PRMSL, CAPE, TCDC, GUST, SPFH
clone_levels = 200 mb, 250 mb, 300 mb, 400 mb, 500 mb, 700 mb, 850 mb, 925 mb, 1000 mb
clone_skip = anom, precip
clone_pieces = false
)MODEL"},
        {"ICON-D2", R"MODEL(
# DWD's ICON-D2: 2.2 km over Germany and the countries round it, a run every 3 hours to 48 hours, hourly (opendata.dwd.de). A file for each field and hour, compressed with bzip2. The upper air
# of this model is only on its own triangular grid (not read here), so this has the surface: the 2 m temperature and dew point, the 10 m wind and gusts, the sea level pressure, the CAPE and
# the cloud cover. Only the last day is kept.
id = ICON-D2
label = DWD ICON-D2 2.2 km (Germany and central Europe)
kind = whole
url = https://opendata.dwd.de/weather/nwp/icon-d2/grib/{cycle}/{file}/icon-d2_germany_regular-lat-lon_single-level_{date}{cycle}_{hour3}_2d_{file}.grib2.bz2
file = when variable=TMP level="2 m above ground" => t_2m
file = when variable=DPT level="2 m above ground" => td_2m
file = when variable=RH level="2 m above ground" => relhum_2m
file = when variable=UGRD level="10 m above ground" => u_10m
file = when variable=VGRD level="10 m above ground" => v_10m
file = when variable=GUST => vmax_10m
file = when variable=PRMSL => pmsl
file = when variable=CAPE => cape_ml
file = when variable=TCDC => clct
file = none
extra_missing = 9999
probe_file = t_2m
probe_hour = 0
cycle_hours = 3
lag_hours = 3
cycles_to_try = 4
max_parallel = 4
sectors = world
hours = 0-48/1
run_every = 3
overlays = false
list = true
clone = true
clone_variables = TMP, DPT, RH, UGRD, VGRD, PRMSL, CAPE, TCDC, GUST
clone_levels = 1 mb
clone_skip = anom, precip
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
        {"RRFS-AK", R"MODEL(
# The RRFS for Alaska (the same open data bucket and runs as the contiguous United States one): 3 km, every hour, to 84 hours from 00, 06, 12 and 18Z. The 2D fields and the pressure levels
# are files of their own; the grid is put on a regular 0.04 degree one.
id = RRFS-AK
label = NOAA/NCEP RRFS Alaska, 3 km
kind = grib
url = https://noaa-rrfs-ops-pds.s3.amazonaws.com/rrfs.{date}/{cycle}/rrfs.t{cycle}z.{file}.f{hour3}.ak.grib2
file_default = 2dfld.3km
file = when level~"^[0-9]+ mb$" => prslev.3km
file = 2dfld.3km
probe_file = 2dfld.3km
cycle_hours = 1
lag_hours = 2
probe_hour = 0
cycles_to_try = 8
warp = 0.04, -180.0, 40.0, -90.0, 75.0
sectors = world
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
        {"RRFS-HI", R"MODEL(
# The RRFS for Hawaii (the same open data bucket and runs as the contiguous United States one): 2.5 km, every hour, to 84 hours from 00, 06, 12 and 18Z. The 2D fields and the pressure levels
# are files of their own; the grid is put on a regular 0.025 degree one.
id = RRFS-HI
label = NOAA/NCEP RRFS Hawaii, 2.5 km
kind = grib
url = https://noaa-rrfs-ops-pds.s3.amazonaws.com/rrfs.{date}/{cycle}/rrfs.t{cycle}z.{file}.f{hour3}.hi.grib2
file_default = 2dfld.2p5km
file = when level~"^[0-9]+ mb$" => prslev.2p5km
file = 2dfld.2p5km
probe_file = 2dfld.2p5km
cycle_hours = 1
lag_hours = 2
probe_hour = 0
cycles_to_try = 8
warp = 0.025, -162.5, 17.5, -153.0, 23.5
sectors = world
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
        {"RRFS-PR", R"MODEL(
# The RRFS for Puerto Rico and the Virgin Islands (the same open data bucket and runs as the contiguous United States one): 2.5 km, every hour, to 84 hours from 00, 06, 12 and 18Z. The 2D fields and the pressure levels
# are files of their own; the grid is put on a regular 0.025 degree one.
id = RRFS-PR
label = NOAA/NCEP RRFS Puerto Rico and the Virgin Islands, 2.5 km
kind = grib
url = https://noaa-rrfs-ops-pds.s3.amazonaws.com/rrfs.{date}/{cycle}/rrfs.t{cycle}z.{file}.f{hour3}.pr.grib2
file_default = 2dfld.2p5km
file = when level~"^[0-9]+ mb$" => prslev.2p5km
file = 2dfld.2p5km
probe_file = 2dfld.2p5km
cycle_hours = 1
lag_hours = 2
probe_hour = 0
cycles_to_try = 8
warp = 0.025, -76.5, 14.0, -62.0, 23.0
sectors = world
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
        {"RRFS-NA", R"MODEL(
# The RRFS for North America (the same open data bucket and runs as the contiguous United States one): 13 km, every hour, to 84 hours, from the 00, 06, 12 and 18Z runs only (the other hours' runs have no North America files). The 2D fields and the pressure levels
# are files of their own; the grid is put on a regular 0.1 degree one.
id = RRFS-NA
label = NOAA/NCEP RRFS North America, 13 km
kind = grib
url = https://noaa-rrfs-ops-pds.s3.amazonaws.com/rrfs.{date}/{cycle}/rrfs.t{cycle}z.{file}.f{hour3}.na.grib2
file_default = 2dfld.13km
file = when level~"^[0-9]+ mb$" => prslev.13km
file = 2dfld.13km
probe_file = 2dfld.13km
cycle_hours = 6
lag_hours = 5
probe_hour = 0
cycles_to_try = 4
warp = 0.1, -180.0, 0.0, -5.0, 85.0
sectors = world
hours = 0-84/1
run_every = 6
on_missing = previous_run
preload_hours = 12
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, RH, UGRD, VGRD, ABSV, DPT, PRMSL, PWAT, CAPE, CIN, APCP, CRAIN, CSNOW, CFRZR, CICEP, TCDC, GUST, SNOD, WEASD, REFC, HLCY, VIS, PRATE
clone_levels = 200 mb, 250 mb, 300 mb, 400 mb, 500 mb, 700 mb, 850 mb, 925 mb, 1000 mb
clone_skip = anom
clone_pieces = false
clone_rename = PRMSL:MSLET
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
        {"CFS-ANL", R"MODEL(
# The CFS analysis (the Climate Data Assimilation System, the CFSR kept going): four analyses a day, 1 degree, on AWS (open data bucket noaa-cfs-pds, 22 April 2023 on, about a day behind). The
# pressure levels, the sea level pressure and the precipitable water are in `pgrblanl`; the 2 m and 10 m fields are in the surface flux file `sfluxgrbf00`. Older analyses (2011 on, and the CFSR
# of 1979-2010) are in NCEI's monthly files, which have no index: not read here yet.
id = CFS-ANL
label = NOAA/NCEP CFS analysis (CDAS), 1 degree
kind = grib
url = https://noaa-cfs-pds.s3.amazonaws.com/cdas.{date}/cdas1.t{cycle}z.{file}.grib2
file_default = pgrblanl
file = when level="2 m above ground" => sfluxgrbf00
file = when level="10 m above ground" => sfluxgrbf00
file = pgrblanl
probe_file = pgrblanl
probe_hour = 0
cycle_hours = 6
lag_hours = 8
cycles_to_try = 8
max_parallel = 4
warp = 1.0, -180.0, -90.0, 180.0, 90.0
sectors = world
hours = 0-0/1
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, UGRD, VGRD, SPFH, PRMSL, PWAT, RH
# (the fields the NCEI monthly files have, so that the same charts draw for 1979 on)
clone_fields = HGT|1000 mb, HGT|850 mb, HGT|700 mb, HGT|500 mb, HGT|200 mb, TMP|850 mb, TMP|700 mb, TMP|500 mb, TMP|250 mb, UGRD|850 mb, UGRD|700 mb, UGRD|500 mb, UGRD|250 mb, VGRD|850 mb, VGRD|700 mb, VGRD|500 mb, VGRD|250 mb, SPFH|850 mb, SPFH|700 mb, SPFH|500 mb, RH|850 mb, RH|700 mb, RH|500 mb
clone_skip = precip, thick, 850_vort_ht, 500_vort_ht
clone_pieces = false
# relative humidity is worked out from the specific humidity of the level and its temperature
clone_specific_humidity = true
)MODEL"},
        {"CFS", R"MODEL(
# NOAA's Climate Forecast System version 2 on AWS (open data bucket noaa-cfs-pds, 31 October 2018 on): a nine month forecast four times a day, 1 degree, 6 hourly. Each variable is a file of its own
# (time_grib_01 is the first member) that holds every forecast hour as a record, found by the index. The grid is a Gaussian one, put on a regular 1 degree grid. Temperatures come in degrees C.
id = CFS
label = NOAA/NCEP CFSv2 seasonal forecast, member 1, 1 degree
kind = grib
url = https://noaa-cfs-pds.s3.amazonaws.com/cfs.{date}/{cycle}/time_grib_01/{file}.01.{date}{cycle}.daily.grb2
forecast_text = {hour} hour fcst
file_default = tmp2m
file = when variable=TMP level="2 m above ground" => tmp2m
file = when variable=TMAX => tmax
file = when variable=TMIN => tmin
file = when variable=PRMSL => prmsl
file = when variable=PWAT => pwat
file = when variable=PRATE => prate
file = when variable=UGRD,VGRD level="10 m above ground" => wnd10m
file = when variable=HGT level~"^[0-9]+ mb$" => z{mb}
file = when variable=TMP level~"^[0-9]+ mb$" => t{mb}
file = when variable=UGRD,VGRD level~"^[0-9]+ mb$" => wnd{mb}
file = when variable=SPFH level~"^[0-9]+ mb$" => q{mb}
file = when variable=VVEL level="500 mb" => vvel500
probe_file = tmp2m
probe_hour = 6
cycle_hours = 6
lag_hours = 8
cycles_to_try = 6
max_parallel = 4
warp = 1.0, -180.0, -90.0, 180.0, 90.0
sectors = world
hours = 6-240/6, 264-6552/24
preload_hours = 24
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, UGRD, VGRD, PRMSL, PWAT, SPFH, RH
# only the fields the bucket has (checked for 404: no 250 mb height, 925 mb temperature or 250 / 1000 mb humidity)
clone_fields = HGT|1000 mb, HGT|850 mb, HGT|700 mb, HGT|500 mb, HGT|200 mb, TMP|850 mb, TMP|700 mb, TMP|500 mb, TMP|250 mb, UGRD|850 mb, UGRD|700 mb, UGRD|500 mb, UGRD|250 mb, VGRD|850 mb, VGRD|700 mb, VGRD|500 mb, VGRD|250 mb, SPFH|850 mb, SPFH|700 mb, SPFH|500 mb, RH|850 mb, RH|700 mb, RH|500 mb
on_missing = previous_run
clone_skip = precip, thick, 850_vort_ht, 500_vort_ht
clone_pieces = false
# relative humidity is worked out from the specific humidity of the level and its temperature
clone_specific_humidity = true
)MODEL"},
        {"HRRR-AK", R"MODEL(
# The 3 km High-Resolution Rapid Refresh for Alaska (the HRRR bucket's alaska folder): a run every 3 hours to 48 hours, the 2D fields (sfc) and the pressure levels (prs) in files of their own. The
# polar stereographic grid is put on a regular 0.04 degree one east of the date line (the western Aleutians are cut).
id = HRRR-AK
label = NOAA/NCEP HRRR Alaska 3 km
kind = grib
url = https://noaa-hrrr-bdp-pds.s3.amazonaws.com/hrrr.{date}/alaska/hrrr.t{cycle}z.wrf{file}f{hour2}.ak.grib2
file_default = sfc
file = when level~"^[0-9]+ mb$" => prs
file = sfc
probe_file = sfc
cycle_hours = 3
lag_hours = 3
probe_hour = 0
cycles_to_try = 6
warp = 0.04, -180.0, 40.0, -90.0, 75.0
sectors = world
hours = 0-48/1
run_every = 3
on_missing = previous_run
preload_hours = 12
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, RH, UGRD, VGRD, ABSV, DPT, PRMSL, PWAT, CAPE, CIN, APCP, CRAIN, CSNOW, CFRZR, CICEP, TCDC, GUST, SNOD, WEASD, REFC, HLCY, VIS, PRATE
clone_levels = 200 mb, 250 mb, 300 mb, 400 mb, 500 mb, 700 mb, 850 mb, 925 mb, 1000 mb
clone_skip = anom
clone_pieces = false
clone_rename = PRMSL:MSLMA
)MODEL"},
        {"ICON-EU", R"MODEL(
# DWD's ICON-EU: 6.5 km over Europe on a regular latitude / longitude grid, a run every 3 hours, hourly to 78 hours (opendata.dwd.de). A file for each field, hour and level, compressed with bzip2.
# Heights come as geopotential (m2/s2), the temperatures in Kelvin (read as degrees C).
id = ICON-EU
label = DWD ICON-EU 6.5 km (Europe)
kind = whole
url = when file~"^[a-z]+/[0-9]+_" => https://opendata.dwd.de/weather/nwp/icon-eu/grib/{cycle}/{dir}/icon-eu_europe_regular-lat-lon_pressure-level_{date}{cycle}_{hour3}_{name}.grib2.bz2
url = https://opendata.dwd.de/weather/nwp/icon-eu/grib/{cycle}/{dir}/icon-eu_europe_regular-lat-lon_single-level_{date}{cycle}_{hour3}_{name}.grib2.bz2
file = when variable=HGT level~"^[0-9]+ mb$" => fi/{mb}_FI
file = when variable=TMP level~"^[0-9]+ mb$" => t/{mb}_T
file = when variable=UGRD level~"^[0-9]+ mb$" => u/{mb}_U
file = when variable=VGRD level~"^[0-9]+ mb$" => v/{mb}_V
file = when variable=RH level~"^[0-9]+ mb$" => relhum/{mb}_RELHUM
file = when variable=VVEL level~"^[0-9]+ mb$" => omega/{mb}_OMEGA
file = when variable=TMP level="2 m above ground" => t_2m/T_2M
file = when variable=DPT level="2 m above ground" => td_2m/TD_2M
file = when variable=RH level="2 m above ground" => relhum_2m/RELHUM_2M
file = when variable=UGRD level="10 m above ground" => u_10m/U_10M
file = when variable=VGRD level="10 m above ground" => v_10m/V_10M
file = when variable=GUST => vmax_10m/VMAX_10M
file = when variable=PRMSL => pmsl/PMSL
file = when variable=CAPE => cape_ml/CAPE_ML
file = when variable=TCDC => clct/CLCT
file = none/none
value_scale = HGT:0.1019716
extra_missing = 9999
probe_file = t_2m/T_2M
probe_hour = 0
cycle_hours = 3
lag_hours = 4
cycles_to_try = 4
max_parallel = 4
sectors = world
hours = 0-78/1
run_every = 3
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, RH, UGRD, VGRD, DPT, PRMSL, CAPE, TCDC, GUST, VVEL
clone_levels = 200 mb, 250 mb, 300 mb, 400 mb, 500 mb, 700 mb, 850 mb, 925 mb, 1000 mb
clone_skip = anom, precip
clone_pieces = false
)MODEL"},
        {"HRRR", R"MODEL(
# the 3 km High-Resolution Rapid Refresh on AWS (NOAA's open data bucket, kept from mid 2014): the 2D fields (sfc) and the pressure levels (prs) are files of their own
id = HRRR
label = NOAA/NCEP HRRR 3 km
kind = grib
url = https://noaa-hrrr-bdp-pds.s3.amazonaws.com/hrrr.{date}/conus/hrrr.t{cycle}z.wrf{file}f{hour2}.grib2
file_default = sfc
file = when level~"^[0-9]+ mb$" => prs
file = sfc
probe_file = sfc
cycle_hours = 1
lag_hours = 2
probe_hour = 0
cycles_to_try = 8
warp = 0.03, -127.0, 22.0, -65.0, 52.0
sectors = conus
hours = 0-48/1
hourly_runs = true
on_missing = previous_run
preload_hours = 12
run_length = every 6 : 48 , else : 18
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, RH, UGRD, VGRD, ABSV, DPT, PRMSL, PWAT, CAPE, CIN, APCP, CRAIN, CSNOW, CFRZR, CICEP, TCDC, GUST, SNOD, WEASD, REFC, HLCY, VIS, PRATE
clone_levels = 200 mb, 250 mb, 300 mb, 400 mb, 500 mb, 700 mb, 850 mb, 925 mb, 1000 mb
clone_skip = anom
clone_pieces = false
clone_rename = PRMSL:MSLMA
)MODEL"},
        {"ERA5", R"MODEL(
# ECMWF's ERA5 reanalysis, hourly, 0.25 degree, global, January 1940 to March 2023 (the NCAR copy in the open data bucket nsf-ncar-era5): NetCDF files for each variable, a month of hours for the
# surface fields and a day of hours and 37 pressure levels for the upper air (1 GB a file, read by range: a field is a few MB). Every hour is an analysis; the run is the hour itself.
id = ERA5
check_run = 2020061512
label = ECMWF ERA5 reanalysis (NCAR archive), 0.25 degree
group = Archive
kind = netcdf
url = https://nsf-ncar-era5.s3.amazonaws.com/e5.oper.an.{file}.nc
cycle_hours = 1
lag_hours = 1
probe_hour = 0
cycles_to_try = 1
sectors = world
hours = 0-0/1
hourly_runs = true
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, RH, UGRD, VGRD, SPFH, VVEL, DPT, PRMSL, PWAT, CAPE, TCDC
clone_levels = 10 mb, 50 mb, 100 mb, 200 mb, 250 mb, 300 mb, 400 mb, 500 mb, 700 mb, 850 mb, 925 mb, 1000 mb
clone_skip = anom, precip
clone_pieces = false
)MODEL"},
        {"GFS-ANL", R"MODEL(
# The GFS analysis of the years before the open data bucket (2007 to mid 2014 with an index file; NCEI's historical folder), 0.5 degree, four a day: the analysis is the only hour. The 1 degree
# GRIB1 files of 2004-2007 are not read here. A 00Z analysis is missing on some days (27 April 2011, say): that run shows an error.
id = GFS-ANL
check_run = 2012061512
label = NOAA/NCEP GFS analysis (NCEI archive), 0.5 degree
group = Archive
kind = grib
url = https://www.ncei.noaa.gov/oa/prod-model/global-forecast-system/access/historical/analysis/{year}{month}/{date}/gfsanl_4_{date}_{cycle}00_000.grb2
index_replaces = .grb2
index_suffix = .inv
cycle_hours = 6
lag_hours = 6
probe_hour = 0
cycles_to_try = 2
sectors = world
hours = 0-0/1
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, RH, UGRD, VGRD, ABSV, DPT, PRMSL, PWAT, CAPE, CIN, VVEL
clone_levels = 100 mb, 200 mb, 250 mb, 300 mb, 400 mb, 500 mb, 700 mb, 850 mb, 925 mb, 1000 mb
clone_skip = anom, precip
clone_pieces = false
)MODEL"},
        {"GFS1-ANL", R"MODEL(
# The 1 degree GFS analysis of NCEI's archive, 4 March 2004 to 15 May 2020, four a day, with the analysis and its 3 and 6 hour forecasts in each cycle's files. GRIB1 until 7 April 2017 (wgrib
# inventories, NceiInventory), GRIB2 after. Where the 0.5 degree GFS-ANL has the years (2007 to mid 2014) that one is finer.
id = GFS1-ANL
check_run = 2010061512
label = NOAA/NCEP GFS analysis (NCEI archive), 1 degree
group = Archive
kind = grib
url = https://www.ncei.noaa.gov/oa/prod-model/global-forecast-system/access/historical/analysis/{year}{month}/{date}/gfsanl_3_{date}_{cycle}00_{hour3}.grb
index_replaces = .grb2|.grb
index_suffix = .inv
cycle_hours = 6
lag_hours = 6
probe_hour = 0
cycles_to_try = 2
sectors = world
hours = 0-6/3
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, RH, UGRD, VGRD, ABSV, DPT, PRMSL, PWAT, CAPE, CIN, VVEL
clone_levels = 100 mb, 200 mb, 250 mb, 300 mb, 400 mb, 500 mb, 700 mb, 850 mb, 925 mb, 1000 mb
clone_skip = anom, precip
clone_pieces = false
)MODEL"},
        {"RTMA", R"MODEL(
# The Real-Time Mesoscale Analysis, hourly, from the open data bucket noaa-rtma-pds (19 March 2013 on). The 2.5 km CONUS Lambert grid is put on a regular 0.03 degree one. Surface fields only: the 2 m temperature and dew point, the 10 m wind and gust, the visibility and the cloud cover (the charts are
# this model's own, in GfsChart.cpp). Before May 2017 there is no index file: one is made from the head of each message and kept.
id = RTMA
label = NOAA/NCEP RTMA surface analysis, 2.5 km
group = Archive
kind = grib
url = https://noaa-rtma-pds.s3.amazonaws.com/rtma2p5.{date}/rtma2p5.t{cycle}z.2dvaranl_ndfd.grb2_wexp
index_suffix = .idx
cycle_hours = 1
lag_hours = 1
probe_hour = 0
cycles_to_try = 3
warp = 0.03, -135.0, 20.0, -60.0, 55.0
sectors = conus
hours = 0-0/1
hourly_runs = true
overlays = true
list = true
)MODEL"},
        {"URMA", R"MODEL(
# The UnRestricted Mesoscale Analysis (the RTMA made again with later observations), hourly, from the open data bucket noaa-urma-pds (29 January 2014 on). The 2.5 km CONUS Lambert grid is put on a regular 0.03 degree one. Surface fields only: the 2 m temperature and dew point, the 10 m wind and gust, the visibility and the cloud cover (the charts are
# this model's own, in GfsChart.cpp). Before May 2017 there is no index file: one is made from the head of each message and kept.
id = URMA
label = NOAA/NCEP URMA surface analysis, 2.5 km
group = Archive
kind = grib
url = https://noaa-urma-pds.s3.amazonaws.com/urma2p5.{date}/urma2p5.t{cycle}z.2dvaranl_ndfd.grb2_wexp
index_suffix = .idx
cycle_hours = 1
lag_hours = 6
probe_hour = 0
cycles_to_try = 8
warp = 0.03, -135.0, 20.0, -60.0, 55.0
sectors = conus
hours = 0-0/1
hourly_runs = true
overlays = true
list = true
)MODEL"},
        {"NARR", R"MODEL(
# The North American Regional Reanalysis, 32 km, every 3 hours from January 1979 to September 2014 (NCEI's archive; GRIB1 with wgrib inventories). A file of 55 MB for each analysis. The Lambert grid is
# put on a regular 0.25 degree one. The pressure levels are every 25 mb; the fields the charts need at them are height, temperature, wind and humidity (no relative humidity or vorticity).
id = NARR
check_run = 2010061512
label = NOAA/NCEP North American Regional Reanalysis (NCEI archive), 32 km
group = Archive
kind = grib
url = https://www.ncei.noaa.gov/oa/prod-model/north-american-regional-reanalysis/access/3-hourly/{year}{month}/{date}/narr-a_221_{date}_{cycle}00_000.grb
index_replaces = .grb
index_suffix = .inv
cycle_hours = 3
run_every = 3
lag_hours = 3
probe_hour = 0
cycles_to_try = 2
warp = 0.25, -175.0, 5.0, -45.0, 80.0
sectors = conus
hours = 0-0/1
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, UGRD, VGRD, SPFH, DPT, PRMSL, PWAT, CAPE, CIN, HLCY, RH
clone_fields = HGT|1000 mb, HGT|850 mb, HGT|700 mb, HGT|500 mb, HGT|300 mb, HGT|250 mb, HGT|200 mb, TMP|850 mb, TMP|700 mb, TMP|500 mb, TMP|300 mb, TMP|250 mb, TMP|200 mb, UGRD|850 mb, UGRD|700 mb, UGRD|500 mb, UGRD|300 mb, UGRD|250 mb, UGRD|200 mb, VGRD|850 mb, VGRD|700 mb, VGRD|500 mb, VGRD|300 mb, VGRD|250 mb, VGRD|200 mb, SPFH|850 mb, SPFH|700 mb, SPFH|500 mb, TMP|2 m above ground, DPT|2 m above ground, UGRD|10 m above ground, VGRD|10 m above ground, PRMSL|mean sea level, CAPE|surface, CIN|surface, PWAT|entire atmosphere (considered as a single layer), HLCY|3000-0 m above ground, RH|850 mb, RH|700 mb, RH|500 mb
clone_skip = anom, precip, thick, 850_vort_ht, 500_vort_ht
clone_pieces = false
# relative humidity is worked out from the specific humidity of the level and its temperature
clone_specific_humidity = true
)MODEL"},
        {"NAM-ANL", R"MODEL(
# The NAM analysis of the years before the open data bucket (4 March 2004 to April 2020; NCEI's historical folder), 12 km, four cycles a day, each cycle's file for the analysis and its 3 and 6 hour
# forecasts. GRIB1 files with wgrib inventories (NceiInventory puts them in NOAA's form). The Lambert grid is put on a regular 0.1 degree one.
id = NAM-ANL
check_run = 2015061512
label = NOAA/NCEP NAM analysis (NCEI archive), 12 km
group = Archive
kind = grib
url = https://www.ncei.noaa.gov/oa/prod-model/north-american-mesoscale-model/access/historical/analysis/{year}{month}/{date}/namanl_218_{date}_{cycle}00_{hour3}.grb
index_replaces = .grb
index_suffix = .inv
cycle_hours = 6
lag_hours = 6
probe_hour = 0
cycles_to_try = 2
warp = 0.1, -152.0, 12.0, -49.0, 62.0
sectors = conus
hours = 0-6/3
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, RH, UGRD, VGRD, DPT, PRMSL, PWAT, CAPE, CIN, REFC, HLCY, VVEL, ABSV
clone_levels = 200 mb, 250 mb, 300 mb, 400 mb, 500 mb, 700 mb, 850 mb, 925 mb, 1000 mb
clone_skip = anom, precip
clone_pieces = false
)MODEL"},
        {"RAP-ANL", R"MODEL(
# The hourly analysis of the Rapid Update Cycle (to April 2012) and the Rapid Refresh that followed it, 13 km over North America, from NCEI's model archive (5 January 2005 on; the
# open data bucket takes over in 2021; NCEI's own folder goes on to the present, with its files from 16 May 2020 on). A file of 20 MB for every hour, with its own index. The grid is a Lambert one, put on a regular 0.1 degree one. The only forecast hour is the analysis.
id = RAP-ANL
check_run = 2015061512
label = NOAA/NCEP RUC / RAP hourly analysis (NCEI archive), 13 km
group = Archive
kind = grib
url = https://www.ncei.noaa.gov/oa/prod-model/rapid-refresh/access/historical/analysis/{year}{month}/{date}/rap_130_{date}_{cycle}00_000.grb2
index_replaces = .grb2
index_suffix = .inv
cycle_hours = 1
lag_hours = 1
probe_hour = 0
cycles_to_try = 2
warp = 0.1, -142.0, 15.0, -50.0, 58.0
sectors = conus
hours = 0-0/1
hourly_runs = true
overlays = true
list = true
clone = true
clone_variables = HGT, TMP, RH, UGRD, VGRD, DPT, MSLMA, PWAT, CAPE, CIN, REFC, HLCY, GUST, VIS, SPFH
clone_levels = 200 mb, 250 mb, 300 mb, 400 mb, 500 mb, 700 mb, 850 mb, 925 mb, 1000 mb
clone_skip = anom, precip
clone_pieces = false
clone_rename = PRMSL:MSLMA
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
