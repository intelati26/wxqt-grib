# Crops in drought

The **Crops** tab of the drought window shows how much of each crop, and of the livestock, lies in drought, the way the
Department of Agriculture's weekly *Agriculture in Drought* report works it out. It is the program's own calculation: it is not
a figure published by the Department or by NASS, though it follows the report closely (see **How close** below).

## What the table says

For the week chosen at the top of the window (and the area chosen there: the whole country, a state, a forecast office, a
county): the share of the **production** of each commodity that is in the Drought Monitor's category **D0 or worse, D1 or
worse, D2 or worse, D3 or worse**, and in **D4**, and the change of the D1 share since the week before. Hay and alfalfa are counted
by acres, livestock by head (inventory). Cotton is upland cotton and cattle are cattle without cows (milk cows have a row
of their own): that is what the report counts.

## How it is made

A county counts as in drought when the Monitor's category **at the county's center point** is that category or worse. A
commodity's share is the production of the counties in drought over the production of all the counties. The production of each
county comes from the **Census of Agriculture** (every five years: 2017 and 2022), and the Monitor is read from the week's map
on a fine grid (0.01 degree, about a kilometer). The report switched from the 2017 census to the 2022 one in 2024; so does the
program (the weeks before July 2024 use 2017). Counties whose number the Census withholds (a farm or two) are left out.
Alaska, Hawaii and the territories are not counted, as in the report.

## The line

Under the table, one commodity over a range of weeks (**from** and **to**; the last 52 weeks to begin with, back to January 2000
at the longest): the share in D0 or worse down to D4 for the area chosen at the top. Each week's map is fetched once (the
Monitor's own file of the week, a few hundred KB), made small, and kept; a long range takes a while the first time, and the
title says how many weeks have been read. Choosing another range or area drops the one being read.

## How close

Against the report's own figures for the same weeks (19 commodities, four weeks from 2019 to 2026) the typical difference is
**half a point** and nine in ten are within two points; the worst was four and a half (a commodity of a few heavy counties
whose center lies on the edge of a category). The report itself rounds to whole percents.

## What is kept, and where

In the program's data folder, `drought/`: the small form of each week's map (`vector/`, about 20 KB a week), the county shares of
each week (`counties/`, under 2 KB a week), the county tables of the census (`crops/`) and the Census Bureau's list of county
centers. A week is about 20 KB, so every week since 2000 is under 30 MB. **Save as a picture** (right-click the table or the
line) writes it with the sources underneath.

## The NASS key

The county tables come from the **NASS Quick Stats API**, which needs a free key (quickstats.nass.usda.gov/api, "API Key").
Until the key is set as the environment variable `WXQT_NASS_KEY` the tab uses the tables already on the computer and says
when one is missing; each table is fetched once (a table for each commodity and census) and kept. NASS asks that every
program using its API says so: **This product uses the NASS API but is not endorsed or certified by NASS.**

## Sources

U.S. Drought Monitor (National Drought Mitigation Center at the University of Nebraska-Lincoln, USDA and NOAA); USDA NASS
Census of Agriculture; Census Bureau county gazetteer. The method follows the USDA Office of the Chief Economist's *Agriculture in Drought*.
