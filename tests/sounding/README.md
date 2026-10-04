# Sounding engine regression test

`sounding_test` checks `src/sounding/` against SPC's own computed values. Each file in `fixtures/` is an SPC
observed-sounding text product (public-domain US government data, https://www.spc.noaa.gov/exper/soundings/);
after the raw levels it carries SPC's CAPE/CIN/LCL/LFC/EL, shear, SRH, Bunkers motion, composites, DCAPE,
convective temperature and more. The test recomputes all of them from the raw levels and fails when a value
leaves its tolerance (see the table at the top of `sounding_test.cpp`; tolerances are the engine's measured
accuracy, not aspirations).

The fixtures cover big/weak/zero CAPE, strong shear, high terrain, sea level and an effective inflow layer.
Where SPC's output differs from SHARPpy's code the engine follows SPC (listed in `licenses/sharppy-notice.md`);
these fixtures are what keeps those rules from being "simplified" back.

Run it (needs only a C++20 compiler):

    tests/sounding/run.sh

To add a fixture: save `https://www.spc.noaa.gov/exper/soundings/<yyMMddHH>_OBS/<SITE>.txt` into `fixtures/`.
A new fixture that fails means either a real regression or a new SPC-vs-SHARPpy difference to document.
