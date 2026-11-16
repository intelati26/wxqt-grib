# Chart files: your own maps, as data

(The rules every editable file shares, and which file to edit for which job: [editing-rules.md](editing-rules.md).)

A file named `<name>.chart.json` in your models folder (Settings > Model Sources > "Your file..." opens it) adds charts to a
model. You write a text file; you do not need a new version of the program. They show up in the Model Viewer under the model, in the group **Your charts**, and the
catalogue check counts what they read as cataloged. (Design: section 21 of `plans.md`.)

```json
{
  "version": 1,
  "charts": [
    {
      "id": "newsurface1",
      "model": "GFS",
      "label": "NewSurface1: 2 m temperature and 500 mb height",
      "group": "Your charts",
      "fill": { "field": "TMP@2 m above ground", "ramp": "temperature", "title": "2 m temperature",
                "quantity": "temperature", "legend_step": 5 },
      "contours": [ { "field": "HGT@500 mb", "scale": 0.1, "interval": 6, "title": "500 mb height (dam)",
                      "color": "#202020" } ],
      "barbs": { "u": "UGRD@10 m above ground", "v": "VGRD@10 m above ground" }
    }
  ]
}
```
(`docs/examples/newsurface1.chart.json` is this file.)

## What a chart has

| key | meaning |
|---|---|
| `id` | letters, digits, `_`, `-` (60 at most); unique within the model (a built-in chart of that id wins and yours is reported and left out) |
| `model` | the id of a model of the program (`GFS`, `IFS`, `GEOS-FP` ...) |
| `label`, `group` | the name in the picker; `group` is "Your charts" unless you say |
| `fill` | the area display (below); leave it out for a chart of lines only |
| `contours` | a list of lines: `field`, `interval` (above 0), `scale` (grid value times this is the number printed: `0.1` is meters to decameters), `base`, `title`, `color` (`#rrggbb`), `dashed`, `width`, `highs_lows` |
| `barbs` | `{ "u": field, "v": field }`, wind in m/s drawn as barbs in knots |

A chart has a fill, lines or barbs (one at least) and reads twelve fields at most.

## Fields

`VARIABLE@level` in NOAA's names as the model files use them: `TMP@2 m above ground`, `HGT@500 mb`, `UGRD@10 m above ground`,
`PRMSL@mean sea level`, `RH@700 mb`. An optional `|detail` picks a variant record. A model that does not list a field draws
the chart with a message that names it; **Settings > Model Sources** and the catalogue check show what each model really lists
(the "Not cataloged" list is the fields you could chart).

## The fill

One of: `{ "field": F }`, `{ "speed": [u, v] }` (the speed of two fields), `{ "diff": [a, b] }` (a minus b),
`{ "mean": [a, b] }`; then optionally `"scale"` and `"offset"` (value times scale plus offset: `1.94384` turns m/s into knots)
and:

- `ramp`: the colour scale, by name: `temperature` (degrees C), `wind` (knots), `low-wind`, `humidity`, `precipitation` (mm),
  `precipitable-water`, `reflectivity`, `dewpoint`, `sea-level-pressure`, `cape`, `cloud-cover`, `probability`, `vorticity`,
  `divergence`, `theta-e`, `stratosphere-temperature`, `sea-surface-temperature`, `wave-height`, `snowfall`, `shear`,
  `helicity`, `updraft-helicity`, `height-anomaly`, `temperature-departure`, `visibility`, `satellite-ir`, `water-vapor`,
  `uv-index`, `current-speed`. The ramp is in the grid's own units: the GFS temperature is in degrees C, so use `temperature`.
- `quantity`: `temperature` (so the chart follows your units: degrees F or C), `temperature-difference`, `millimeters`,
  `centimeters`, `meters`; leave it out for a number with no unit conversion.
- `title`, `legend_step` (a tick every so many displayed units).

## Making one from a field you found

`tools/make_chart.py` writes a starter chart file for a field, choosing the ramp and the units from the variable where it
knows them (CAPE, temperature, dewpoint, humidity, precipitation, cloud cover, reflectivity, wind, pressure, helicity, vorticity,
snow, soil, waves, visibility):

    python3 tools/make_chart.py GFS "CAPE@surface" --contour "HGT@500 mb" > cape.chart.json
    python3 tools/make_chart.py GEOS-FP "speed:UGRD@10 mb,VGRD@10 mb" --barbs "UGRD@10 mb" "VGRD@10 mb" --id geos_10mb_wind > wind.chart.json
    python3 tools/make_chart.py IFS "TMP@2 m above ground" --catalog catalog.json

`--catalog` takes the catalogue check's `catalog.json` and warns when the model's newest file does not list a field you named
(and shows the levels it does list). It prints a `note:` for anything it had to guess (a variable with no known ramp, a contour
with no known units): look at those, and at the title, before you use the file. Put the result in your models folder and restart.

## Problems

A file that is not valid is not used at all (one bad chart rejects the file) and the reason, with the chart it is in, is in the
list of model problems (`GfsModels::userProblems`, shown where the model files' problems are). Fix the file; the charts are read
when the program starts.

## Not covered

Anything that needs a derived field (potential vorticity, an index, a statistic of members) is still a built-in recipe in code;
a chart file draws fields the model's files carry, with the speed, difference and mean above. A later step publishes chart
files with the project the way the server file is published (plan, section 21).
