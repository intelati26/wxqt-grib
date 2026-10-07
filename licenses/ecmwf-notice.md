# ECMWF open data

The Atlantic hurricane screen reads the tropical cyclone tracks of ECMWF's open real-time forecasts (the IFS ensemble, the AIFS ensemble and the
unperturbed IFS / AIFS runs) from `data.ecmwf.int/forecasts/`. The files are downloaded to the user's own computer when a storm is opened and are
not redistributed with the program.

The data is published by ECMWF under the Creative Commons Attribution 4.0 International licence (CC BY 4.0,
https://creativecommons.org/licenses/by/4.0/), see https://www.ecmwf.int/en/forecasts/datasets/open-data and the LICENCE.txt beside the files.

Attribution shown in the program: "Contains ECMWF open data, CC BY 4.0". ECMWF does not accept any liability for any error or omission in the data,
its availability, or for any loss or damage arising from its use. The data is model output, not an official forecast: for warnings use the National
Hurricane Center and the local National Weather Service office.

`src/hurricane/UtilityEcmwfTracks.cpp` decodes the BUFR files itself (WMO BUFR edition 4, master table version 35, template 3 16 082); the expected
values in `tests/hurricane/` were read with ECMWF's eccodes tools from the same files.
