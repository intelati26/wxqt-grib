# SHARPpy attribution

The sounding analysis code in `src/sounding/` reimplements algorithms from
**SHARPpy** (https://github.com/sharppy/SHARPpy), the open-source port of the
Storm Prediction Center's SHARP/NSHARP sounding software. The functions follow
the structure and formulas of SHARPpy's `sharptab/params.py` (`parcelx`,
`DefineParcel`, `effective_inflow_layer`, `bunkers_storm_motion`, `dcape`,
`stp_fixed`, `stp_cin`, `scp`, `ship`, `mean_theta`, `mean_mixratio`,
`most_unstable_level`) and `sharptab/winds.py` (`helicity`, `mean_wind`,
`wind_shear`, `non_parcel_bunkers_motion`). The thermodynamic core
(`SoundingThermo`) follows `sharptab/thermo.py`. The precipitation-type guess (`SoundingPrecip`) follows `sharptab/watch_type.py`
(`init_phase`, `posneg_temperature`, `best_guess_precip`, adapted there from SHARP code donated by Rich Thompson of SPC).

SHARPpy is distributed under the BSD 3-clause license:

    Copyright (c) 2011, Patrick T. Marsh & John Hart.
    Copyright (c) 2012, MetPy Developers.
    Copyright (c) 2020, Kelton Halbert, Greg Blumberg & Tim Supinie.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name of the MetPy Developers nor the names of any contributors
      may be used to endorse or promote products derived from this software
      without specific permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

## Verification

The engine was checked against SPC's own computed values (the "Parcel
Information" and related blocks) in 270 observed soundings (2026-09-29 00Z to
2026-09-30 12Z). Where SPC's printed numbers differ from SHARPpy's code, the
engine follows SPC, and each such rule is commented in the source:

- DCAPE source layer: layers starting in the lowest 100 mb are skipped
  (258 of 270 exact; the rest are near-ties in the layer means).
- CIN is zeroed only when CAPE is exactly 0 (SHARPpy: when CAPE rounds to 0).
- Mean mixing ratio uses SHARPpy's exact form (mixing ratio of the mean
  dewpoint at the mean pressure); SPC's "0-1 km mean W" is really the lowest
  100 mb (both now match all 270).
- The 3-6 km lapse rate is between 3 and 6 km above mean sea level, as SPC
  prints it (labelled "MSL" on screen); 0-3 km is above ground.
- Convective temperature: see below.
- Melting level / wet-bulb zero: the lowest crossing going up; none when the
  surface is already below 0 C (269 / 268 of 270).

Known, unexplained differences:
- SHIP: on 25 of the 160 soundings with CAPE, SPC's value is lower than the
  published formula gives (never higher); the published formula is used.
- SPC's printed "Effective BWD" is not the quantity SCP/STP use; SHARPpy's
  definition (which reproduces SCP/STP) is implemented.
- Most-unstable parcel start level differs on 33 soundings, all with zero
  MUCAPE (nothing derived from it changes).
- A few soundings show SPC 0-3 km CAPE with zero total CAPE.
- Convective temperature: SHARPpy's convective_temp() iteration, but lifted with
  parcelx, the exact-form mean mixing ratio and CIN >= -1 J/kg, which is what
  SPC's printed values follow (221 of 270 exact, the rest 0.5-1.5 C low).

## SARS databases (added 2026-10-01)

`resourceCreation/sars/sars_supercell.txt` (Rich Thompson, NOAA SPC) and `sars_hail.txt` (Ryan Jewell, NOAA SPC) are copied from
SHARPpy's `sharppy/databases`, and the matching in `src/sounding/SoundingSars.cpp` is a port of `databases/sars.py` (BSD licence,
as above). `src/sounding/SoundingSarsData.cpp` is generated from the two files by `resourceCreation/sars/gen_sars_data.py`.
