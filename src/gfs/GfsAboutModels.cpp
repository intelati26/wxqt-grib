// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsModelFile.h"

// What each built-in model is and why it matters, and where to read more: for the Help screen and the model picker. Written for someone who looks at the weather, not at models.
// Each `link` is `kind | label | url`; kind is wikipedia (an article about the model), background (an article about the idea), docs (the people who run it) or source (where the data is read from).
// A Wikipedia article is linked only where there is one about the model itself.
const std::vector<std::pair<std::string, std::string>>& GfsModelFile::aboutBuiltIn() {
    static const std::vector<std::pair<std::string, std::string>> list{
        {"GEOS-FP", R"ABOUT(
why = NASA's own global forecast system, run by a different centre with a different model from the ones NOAA, ECMWF and Canada run. It is built around the atmosphere's composition as much as its weather: its output carries dust, smoke, sea salt and ozone fields as well, which the weather models do not. Here it has charts of surface dust, dust in the column, dust and total aerosol optical thickness, fine particles (PM2.5), smoke, sulfate and sea salt, in 3 hour averages: the Saharan plume crossing the Atlantic, Asian dust, and the smoke of large fires are what it is for. Its weather charts are a fourth, independent opinion on the large-scale pattern.
link = wikipedia | Goddard Earth Observing System | https://en.wikipedia.org/wiki/Goddard_Earth_Observing_System
link = docs | NASA Global Modeling and Assimilation Office: weather analysis and prediction | https://gmao.gsfc.nasa.gov/weather_prediction/
link = source | NASA Center for Climate Simulation: GEOS-FP forecast files | https://portal.nccs.nasa.gov/datashare/gmao/geos-fp/forecast/
)ABOUT"},
        {"GFS", R"ABOUT(
why = NOAA's global model, run four times a day out to 16 days at 0.25 degrees. It is the free backbone of US forecasting: many other products start from it, so a chart here is the first place a pattern shows up days ahead. It runs on a coarse grid, so it will not resolve individual storms.
link = wikipedia | Global Forecast System | https://en.wikipedia.org/wiki/Global_Forecast_System
link = docs | NCEP Environmental Modeling Center: GFS | https://www.emc.ncep.noaa.gov/emc/pages/numerical_forecast_systems/gfs.php
link = source | NOAA open data on AWS (noaa-gfs-bdp-pds) | https://registry.opendata.aws/noaa-gfs-bdp-pds/
)ABOUT"},
        {"NBM", R"ABOUT(
why = The National Blend of Models is a statistically corrected blend of many models, built to be the starting point for the National Weather Service's own forecasts. Of the models here it is the nearest to what the forecast offices actually issue, and it gives the likelihood of an outcome, not just a single answer.
link = background | Numerical weather prediction | https://en.wikipedia.org/wiki/Numerical_weather_prediction
link = docs | NBM at the NWS Meteorological Development Laboratory | https://vlab.noaa.gov/web/mdl/nbm
link = source | NOAA open data on AWS (noaa-nbm) | https://registry.opendata.aws/noaa-nbm/
)ABOUT"},
        {"AIGFS", R"ABOUT(
why = An experimental NOAA model made by machine learning (it learned from decades of past weather) rather than by solving the physics. It costs a small fraction of the computing of a physical model, so it can be run often; its value here is as a second opinion beside the GFS. It is new and has not been tested in every situation.
link = source | NOAA open data on AWS (GraphCast GFS) | https://registry.opendata.aws/noaa-nws-graphcastgfs-pds/
link = background | Numerical weather prediction | https://en.wikipedia.org/wiki/Numerical_weather_prediction
)ABOUT"},
        {"IFS", R"ABOUT(
why = The European Centre's global model, the benchmark the others are measured against for forecasts of three to ten days. When it and the GFS disagree on a pattern it is worth knowing why. The open data here is the 0.25 degree run, released under a Creative Commons licence.
link = wikipedia | Integrated Forecasting System | https://en.wikipedia.org/wiki/Integrated_Forecasting_System
link = background | European Centre for Medium-Range Weather Forecasts | https://en.wikipedia.org/wiki/European_Centre_for_Medium-Range_Weather_Forecasts
link = source | ECMWF open data | https://www.ecmwf.int/en/forecasts/datasets/open-data
)ABOUT"},
        {"IFS-ENS", R"ABOUT(
why = The European Centre's physical model run 51 times from slightly different starting points (here its 50 perturbed members). How far the members spread is the uncertainty of the forecast, and a chart with every member's line on it shows the outcomes that are still possible. Where the Euro and the GFS ensemble (GEFS) both spread widely the pattern is truly uncertain; where they agree and are tight it is trusted.
link = wikipedia | Ensemble forecasting | https://en.wikipedia.org/wiki/Ensemble_forecasting
link = background | European Centre for Medium-Range Weather Forecasts | https://en.wikipedia.org/wiki/European_Centre_for_Medium-Range_Weather_Forecasts
link = source | ECMWF open data | https://www.ecmwf.int/en/forecasts/datasets/open-data
)ABOUT"},
        {"ENS-ALL", R"ABOUT(
why = Three ensembles run by three centres (ECMWF's 50 members, NOAA's GEFS with 31 and Canada's GEPS with 21), pooled as one set of 102 outcomes for the same valid time. Their models and their starting points differ, so the pool spreads wider where the centres disagree than any one of them does, and agreement among all three is the strongest sign that a pattern is trusted. It is the idea behind NAEFS, the North American Ensemble Forecast System. The spaghetti charts color each centre's members in a family of their own, so a split between the centres is visible at a glance.
link = wikipedia | Ensemble forecasting | https://en.wikipedia.org/wiki/Ensemble_forecasting
link = background | North American Ensemble Forecast System | https://en.wikipedia.org/wiki/North_American_Ensemble_Forecast_System
link = source | ECMWF open data | https://www.ecmwf.int/en/forecasts/datasets/open-data
link = source | NOAA GEFS on AWS | https://registry.opendata.aws/noaa-gefs/
)ABOUT"},
        {"AIFS", R"ABOUT(
why = ECMWF's machine-learning forecast model. It matches the physical model closely at a fraction of the computing, and its mistakes differ from the physical model's, which makes the pair useful together. Treat a fine detail with caution: it is smoother than a physical model.
link = background | European Centre for Medium-Range Weather Forecasts | https://en.wikipedia.org/wiki/European_Centre_for_Medium-Range_Weather_Forecasts
link = source | ECMWF open data | https://www.ecmwf.int/en/forecasts/datasets/open-data
link = background | Numerical weather prediction | https://en.wikipedia.org/wiki/Numerical_weather_prediction
)ABOUT"},
        {"GEFS", R"ABOUT(
why = The GFS run many times from slightly different starting points. How far the runs spread apart is the uncertainty: tight means the pattern is trusted, wide means it is not. The member picker shows any single run, and the chance charts count how many members reach a threshold, which is how forecasts become probabilities.
link = background | Ensemble forecasting | https://en.wikipedia.org/wiki/Ensemble_forecasting
link = docs | NCEP Environmental Modeling Center: GEFS | https://www.emc.ncep.noaa.gov/emc/pages/numerical_forecast_systems/gefs.php
link = source | NOAA open data on AWS (noaa-gefs) | https://registry.opendata.aws/noaa-gefs/
)ABOUT"},
        {"GEPS", R"ABOUT(
why = Canada's global ensemble, 21 members, made by a different centre with a different model. When it and the GEFS agree the confidence is higher than either alone; where they disagree is where the forecast is least certain. Environment Canada serves it openly.
link = background | Canadian Meteorological Centre | https://en.wikipedia.org/wiki/Canadian_Meteorological_Centre
link = background | Ensemble forecasting | https://en.wikipedia.org/wiki/Ensemble_forecasting
link = source | Environment and Climate Change Canada open data (GEPS) | https://eccc-msc.github.io/open-data/msc-data/nwp_geps/readme_geps-datamart_en/
)ABOUT"},
        {"RRFS", R"ABOUT(
why = NOAA's new 3 kilometre forecast system for the continental US, built to replace the Rapid Refresh, the HRRR and several others. At 3 km it can draw individual lines of storms and their rotation, which the global models cannot, and it updates every hour.
link = background | Rapid Refresh (the system the RRFS replaces) | https://en.wikipedia.org/wiki/Rapid_Refresh_(weather_prediction)
link = docs | NOAA GSL: the Rapid Refresh Forecast System | https://gsl.noaa.gov/focus-areas/unified_forecast_system/rrfs
link = source | NOAA open data on AWS (noaa-rrfs) | https://registry.opendata.aws/noaa-rrfs/
)ABOUT"},
        {"REFS", R"ABOUT(
why = Five runs of the RRFS from varied starting points: a storm-scale ensemble. A single 3 km run puts a storm in one place; the members show how much that placement can be trusted, and the ready-made products give the chance of strong storms, heavy rain or snow nearby. It is the successor to the HREF.
link = background | Ensemble forecasting | https://en.wikipedia.org/wiki/Ensemble_forecasting
link = docs | NOAA GSL: the Rapid Refresh Forecast System | https://gsl.noaa.gov/focus-areas/unified_forecast_system/rrfs
link = source | NOAA open data on AWS (noaa-rrfs) | https://registry.opendata.aws/noaa-rrfs/
)ABOUT"},
        {"HREF", R"ABOUT(
why = The High-Resolution Ensemble Forecast pools ten 3 km runs from several models. It has been the standard tool for severe weather and heavy rain outlooks because its probabilities are well tested. It is being replaced by the REFS, and SPC's HREF-based guidance ends in November 2026.
link = background | Ensemble forecasting | https://en.wikipedia.org/wiki/Ensemble_forecasting
link = docs | SPC: HREF | https://www.spc.noaa.gov/exper/href/
link = source | NOMADS (NOAA Operational Model Archive and Distribution System) | https://nomads.ncep.noaa.gov/
)ABOUT"},
        {"GFS-WAVE", R"ABOUT(
why = The sea state the GFS winds produce: significant wave height, period and direction. It matters for marine forecasts, surf, and anywhere swell from a distant storm arrives days before the weather does.
link = docs | NOAA's wave models (WAVEWATCH III) | https://polar.ncep.noaa.gov/waves/
link = source | NOAA open data on AWS (noaa-gfs-bdp-pds) | https://registry.opendata.aws/noaa-gfs-bdp-pds/
)ABOUT"},
        {"GEFS-WAVE", R"ABOUT(
why = The wave model run with the GEFS control member's winds, at 0.25 degrees. A second look at the sea state, a little finer than GFS-Wave; only the control member is shown.
link = docs | NOAA's wave models (WAVEWATCH III) | https://polar.ncep.noaa.gov/waves/
link = source | NOAA open data on AWS (noaa-gefs) | https://registry.opendata.aws/noaa-gefs/
)ABOUT"},
        {"HAFSA", R"ABOUT(
why = NOAA's hurricane model, a version run for each active storm on a grid that moves with it at 2 km. It is one of the main sources of track and intensity guidance, and its wind field and structure are what a forecaster reads when a storm is near land. It only exists while there is a storm.
link = background | Hurricane Weather Research and Forecasting Model (the model HAFS replaces) | https://en.wikipedia.org/wiki/Hurricane_Weather_Research_and_Forecasting_Model
link = docs | National Hurricane Center: model summary | https://www.nhc.noaa.gov/modelsummary.shtml
link = source | NOMADS (NOAA Operational Model Archive and Distribution System) | https://nomads.ncep.noaa.gov/
)ABOUT"},
        {"HAFSB", R"ABOUT(
why = The second version of NOAA's hurricane model, run beside HAFS-A with different settings. Where the two agree on track or intensity the guidance is more trustworthy; where they differ shows how much the answer depends on the model's choices.
link = background | Hurricane Weather Research and Forecasting Model (the model HAFS replaces) | https://en.wikipedia.org/wiki/Hurricane_Weather_Research_and_Forecasting_Model
link = docs | National Hurricane Center: model summary | https://www.nhc.noaa.gov/modelsummary.shtml
link = source | NOMADS (NOAA Operational Model Archive and Distribution System) | https://nomads.ncep.noaa.gov/
)ABOUT"},
        {"SPCPOST", R"ABOUT(
why = Raw model output says how much rain or what wind; forecasters need the chance of a tornado, damaging wind, hail or thunder near a point. The Storm Prediction Center turns the REFS into those probabilities and corrects them against past events, which is the guidance behind its own outlooks.
link = background | Storm Prediction Center | https://en.wikipedia.org/wiki/Storm_Prediction_Center
link = docs | NWS notice 26-77: SPC_POST v3.0 | https://www.weather.gov/media/notification/pdf_2026/scn26_77_Updated_SPC_POST_v3.0_aaa.pdf
link = source | NOMADS: spc_post | https://nomads.ncep.noaa.gov/pub/data/nccf/com/spc_post/prod/
)ABOUT"},
        {"SPCGEFS", R"ABOUT(
why = The same corrected probabilities from the GEFS: a daily thunderstorm chance out to eight days, and machine-learning estimates of severe weather. They reach further ahead than the storm-scale guidance, which is where an outlook for days four to eight begins.
link = background | Storm Prediction Center | https://en.wikipedia.org/wiki/Storm_Prediction_Center
link = docs | NWS notice 26-77: SPC_POST v3.0 | https://www.weather.gov/media/notification/pdf_2026/scn26_77_Updated_SPC_POST_v3.0_aaa.pdf
link = source | NOMADS: spc_post | https://nomads.ncep.noaa.gov/pub/data/nccf/com/spc_post/prod/
)ABOUT"},
        {"SFCOA", R"ABOUT(
why = The SPC mesoanalysis: an hourly picture of the storm environment (instability, shear, helicity, the tornado and hail parameters) made by correcting a model background with the latest surface observations. It is what forecasters look at to decide whether storms that form will be organised, so it is the first stop on a severe weather day.
link = background | Storm Prediction Center | https://en.wikipedia.org/wiki/Storm_Prediction_Center
link = background | Convective available potential energy | https://en.wikipedia.org/wiki/Convective_available_potential_energy
link = background | Significant tornado parameter | https://en.wikipedia.org/wiki/Significant_tornado_parameter
link = docs | SPC Mesoanalysis | https://www.spc.noaa.gov/exper/mesoanalysis/
link = source | NOMADS: spc_post (SFCOA-HRRR) | https://nomads.ncep.noaa.gov/pub/data/nccf/com/spc_post/prod/
)ABOUT"},
        {"RRFSD", R"ABOUT(
why = The mesoanalysis fields, computed from RRFS forecasts instead of observations: the same instability and shear charts, for the next three days. It answers the question the analysis cannot: where the ingredients are expected to come together, and when.
link = background | Storm Prediction Center | https://en.wikipedia.org/wiki/Storm_Prediction_Center
link = background | Convective available potential energy | https://en.wikipedia.org/wiki/Convective_available_potential_energy
link = docs | NWS notice 26-77: SPC_POST v3.0 | https://www.weather.gov/media/notification/pdf_2026/scn26_77_Updated_SPC_POST_v3.0_aaa.pdf
link = source | NOMADS: spc_post (RRFSD) | https://nomads.ncep.noaa.gov/pub/data/nccf/com/spc_post/prod/
)ABOUT"},
        {"SPCREFS", R"ABOUT(
why = The Storm Prediction Center's own view of the REFS: means, maxima, chances, and paintballs that draw each member's storms in its own colour so you can see where they agree. These are the products its forecasters use for the next two days of convection.
link = background | Storm Prediction Center | https://en.wikipedia.org/wiki/Storm_Prediction_Center
link = background | Ensemble forecasting | https://en.wikipedia.org/wiki/Ensemble_forecasting
link = docs | SPC REFS viewer | https://www.spc.noaa.gov/exper/refs/viewer
link = source | SPC REFS open data (Zarr) | https://www.spc.noaa.gov/exper/refs/data/
)ABOUT"},
    };
    return list;
}
