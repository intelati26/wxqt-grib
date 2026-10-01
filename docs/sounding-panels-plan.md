# Sounding viewer: remaining SPC panels (sketch, 2026-10-01)

Done: skew-T on SPC's axes, hodograph, 1 km / 6 km wind inset, grid tables, annotations (precip type, layer labels),
wind speed vs height bars (`drawWindSpeed`, panel rect 595,25,93,565).

All remaining panels are ports from SHARPpy (BSD, see `sharppy-notice.md`). Never guess a formula; port the source.

## Next: inferred temperature advection column (rect 688,25,67,565)
Source: `sharptab/params.py: inferred_temp_adv(prof, dp=-100, lat=35)`, panel `viz/advection.py`.
- Layers every 100 mb from the surface up to 100 mb: for each pair (p_bottom, p_top)
  - mean wind of the layer (mass-weighted `winds.mean_wind`), geostrophic, speed in m/s
  - `d_theta` = change of wind direction with height: rotate so the bottom direction is 180, then
    `d_theta = top_dir - 180` (wrapped to 0..360 first)
  - `multiplier = (f / 9.81) * (pi / 180)`, `f = 2 * omega * sin(lat)`, `omega = 2 pi / 86164`
  - `t_adv = multiplier * speed^2 * avg_temp * d_theta / (z_top - z_bottom)` K/s, times 3600 for C/hr
    (SHARPpy's `avg_temp = (T_bottom + T_top) * 2` in K is kept as written, not "fixed")
- Panel: x scale -13..+13 C/hr (SHARPpy `adv_min/adv_max`), dashed centre line at 0, one bar per layer on the skew-T's
  pressure scale; warm advection red, cold advection `#3399CC`, value printed beside each bar; title "Inf. Temp. Adv. (C/hr)".
- Needs: station latitude. Observed soundings: SoundingSites has lat/lon; model soundings: the picked point's `lat`.
  `SoundingProfile` gets a `latitude` field (default 35 as SHARPpy does). Note: SHARPpy's values are much smaller than SPC's
  (documented in its source); label it as inferred.
- New code: `src/sounding/SoundingAdvection.{h,cpp}` (pure function + layer struct, unit-checkable against a hand case),
  `drawTempAdvection()` in `SoundingViewer.cpp` next to `drawWindSpeed()`.

## After that
1. theta-e vs pressure with TEI (`viz/thetae.py`, `params.py: thetae_diff`/TEI) in the strip under the wind panels.
2. storm-relative winds vs height with the classic-supercell envelope (`viz/srwinds.py`).
3. SHIP and effective-STP box plots (`viz/ship.py`, `viz/stpef.py`; the percentile data is in those files).
4. SARS analogs (`viz/analogues.py`, `databases/sars_supercell.txt`, `sars_hail.txt`, matching in `sars.py`).
5. SPC-style bottom tables (MU parcel row, K index, MidRH/LowRH, SigSevere, ESP, MMP, WNDG, NCAPE, Corfidi vectors, MPL).
Each panel is one commit: port, draw, headless screenshot (`WXQT_OPEN=spcsoundings.png WXQT_GRAB=...`), compare with SPC's graphic.
