# ICON

*DWD ICON global 13 km (via Open-Meteo)*

## Why it matters

The German Weather Service's global model, run four times a day on a grid of triangles that covers the sphere evenly (here put on a regular 0.125 degree grid, about 13 km). Outside Europe it is the one to set beside the GFS and the Euro, and its 00Z and 12Z runs go out 7 days.

Over Europe DWD's finer ICON-EU and ICON-D2 show more; this is the same family for the rest of the world.

## How it compares with the other ICON models

| Model | Grid | Covers | Forecast length |
|---|---|---|---|
| ICON (this one) | 0.125 degrees, about 13 km | the world | 120 h, 180 h on the 00Z and 12Z runs |
| ICON-EU | about 6.5 km | Europe | about 120 h |
| ICON-D2 | about 2.2 km | Germany and central Europe | 48 h |

The global model also has the pressure levels (1000 to 50 hPa), which ICON-D2 does not.

## Further reading

- [Numerical weather prediction](https://en.wikipedia.org/wiki/Numerical_weather_prediction) (Wikipedia)

## Where the data is read from

- [Open-Meteo: DWD ICON](https://open-meteo.com/en/docs/dwd-api)
