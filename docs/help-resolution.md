# What is model resolution?

A weather model does not know the weather everywhere. It knows it at a **grid of points**, and each value stands for the whole cell round its point. The **resolution** is how far apart the points are.

The picture below draws one dot for each point of six grids over the same 1.4 by 1.1 degree box, centred on the Four Corners, where Utah, Colorado, Arizona and New Mexico meet. The 20 km bar is the same in every panel.

| Grid | Spacing | Points in the box | Used by |
|---|---|---|---|
| 0.5 degree | about 55 km | 9 | the GEFS ensemble, the GEPS, the JMA GSM |
| 0.25 degree | about 28 km | 25 | the GFS, the GEOS-FP, the AIFS, the ARPEGE |
| 0.1 degree | about 11 km | 154 | the ECMWF IFS (open data), the ARPEGE Europe |
| 13 km (by name) | about 12 km | 110 | the RRFS North America (13 km), the WOOF-NAM (12 km; NCEP retired the NAM itself on 3 Nov 2026) |
| 3 km | 3 km | 1,687 | the HRRR, the RRFS (CONUS and Alaska) |
| 1.5 km | 1.27 km | 4,119 (the nest covers only part of the box) | the RRFS fire weather nest |

![Six grids over the Four Corners: 0.5, 0.25 and 0.1 degree, 13 km, 3 km and 1.5 km](:/help/guides/images/resolution-ladder.png)

## Reading the picture

* **Two kinds of grid.** The three degree grids are straight rows and columns of latitude and longitude. The 13 km grid is on a rotated latitude/longitude grid and the 3 and 1.5 km ones on a **Lambert conformal** map, where every cell is the same size on the ground; their rows are tilted against the state lines.
* **Names do not sort by size.** A 0.1 degree grid is about 11 km, so it is finer than the "13 km" RRFS North America grid (0.108 degrees, about 12 km): 154 points in the box against 110. Compare the spacing, not the label.
* **Each value is a cell, not a place.** A state line often runs between two points, and the point on one side says nothing about the other. At 0.25 degrees the whole box is only 25 numbers.
* **Counts grow with the square of the spacing.** Halve the distance and the points quadruple (a 3 km grid has 1,687 points in the box; a full 1.5 km one would have about four times as many), which is why the fine models cover a country or a region and run for fewer hours.

## What it means on a chart

A feature smaller than about three cells is not drawn. A thunderstorm is 10 to 20 km wide: one dot of a 0.5 degree grid, a handful at 3 km. A valley 5 km wide is invisible at 28 km. Finer shows more detail, but not always more truth: see the model's own page for what it handles well.

*Every panel is drawn from the cell positions of a real file (the GEFS, GFS, ECMWF IFS open data, RRFS North America, HRRR and RRFS fire weather nest runs of 17 November 2026). The RRFS North America grid is a rotated latitude/longitude grid, so its rows are tilted too. The fire weather nest is moved by the forecasters over the part of the West at risk (over Utah that day), so it covers only the top of the Four Corners box: the blank part is outside the model.*
