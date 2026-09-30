# SHARPpy attribution

The sounding analysis code in `src/sounding/` reimplements algorithms from
**SHARPpy** (https://github.com/sharppy/SHARPpy), the open-source port of the
Storm Prediction Center's SHARP/NSHARP sounding software. The functions follow
the structure and formulas of SHARPpy's `sharptab/params.py` (`parcelx`,
`DefineParcel`, `effective_inflow_layer`, `bunkers_storm_motion`, `dcape`,
`stp_fixed`, `stp_cin`, `scp`, `ship`, `mean_theta`, `mean_mixratio`,
`most_unstable_level`) and `sharptab/winds.py` (`helicity`, `mean_wind`,
`wind_shear`, `non_parcel_bunkers_motion`). The thermodynamic core
(`SoundingThermo`) follows `sharptab/thermo.py`.

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
2026-09-30 12Z). Known differences: DCAPE matches SPC on some soundings and is
off on others; SPC's printed "Effective BWD" is not the quantity SCP/STP use
(SHARPpy's definition is implemented); convective temperature is not ported.
