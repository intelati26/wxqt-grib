# Ensembles and variables: `ensembles.json`

The windows that show an ensemble at a place (the percentiles, and two models compared) offer a list of **ensemble models** and a list of **variables** (temperature, wind,
snow ...). Both lists are tables inside the program, and **your own file goes over them**: add a model the service has, add a variable the service has, or change a label,
without waiting for a new version of the program.

(The rules every editable file shares, and which file for which job: [editing-rules.md](editing-rules.md).)

## Where the file goes

`ensembles.json` in the folder of your own model files (Settings > Model Sources > "Your file..." opens that folder; it is the same folder as `servers.json`).
The program reads it when a window opens. A file that is not valid is **not used at all** (the built-in lists stand) and the window says why in its status line.

## The file

```json
{
  "version": 1,
  "models": {
    "bom_access_global_ensemble": {
      "label": "Australian BoM ACCESS-GE (18 members)",
      "map": ""
    }
  },
  "variables": {
    "cape": {
      "label": "CAPE",
      "title": "Convective available potential energy",
      "quantity": "plain",
      "unit": "J/kg",
      "zero_based": true,
      "digits": 0
    }
  }
}
```

- **`models`**: the key is the name the service (Open-Meteo's ensemble service) gives the model (`gfs025`, `ecmwf_ifs025`, `icon_seamless` ...). `label` is what the list shows.
  `map` is the program's own model for the same system (`GEFS`, `IFS-ENS`, `AIFS`, `GEPS`, `ICON`, `UKMO`, `AIGFS`, `ICON-EU`): "Why?" opens that model's maps. Leave it empty
  and "Why?" says there is no map. A key that is already in the list **replaces** that row; a new key goes at the end.
- **`variables`**: the key is the service's name for it (`temperature_2m`, `wind_gusts_10m`, `cape` ...). `label` is the list's text, `title` the chart's heading (the label when left out).
  - `quantity` tells how the number becomes your units: `temperature` (deg C to your setting, and a difference of two temperatures has no offset), `precipitation` (mm to in),
    `wind` (m/s to mph), `snow` (cm to in), `pressure` (hPa), `percent` (%), or `plain` (no change: say what it is in with `unit`).
  - `accumulate`: true shows the values added up since the first hour (rain, snow). `zero_based`: true starts the axis at zero. `digits`: 0 to 4 decimals in the numbers.
- Names are `a` to `z`, `0` to `9` and `_`; at most 60 models and 60 variables; the file is at most 64 KB; `version` must be 1.

## What can go wrong

- **The service does not know the name**: the window shows the service's own message ("No data is available for this location", "Cannot initialize ..."): a model that covers one region only
  (ICON-EU) says so for a place outside it. Nothing is broken: take the row out, or use it for places it covers.
- **A model does not carry the variable** (many have no gusts): that model is left out of the comparison and named in the status line.
- **Wind arrives in m/s** (the program asks for it so), so a `wind` variable is converted for you; a variable the service gives in other units than the table says needs `plain` and its own unit.
- The free service is for non-commercial use, with attribution: the windows say "members by Open-Meteo".
