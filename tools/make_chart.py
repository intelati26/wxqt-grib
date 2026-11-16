#!/usr/bin/env python3
"""Writes a starter chart file (docs/chart-files.md) for a field you found in the catalogue:

   python3 tools/make_chart.py GFS "CAPE@surface" --contour "HGT@500 mb" > cape.chart.json
   python3 tools/make_chart.py GEOS-FP "speed:UGRD@10 mb,VGRD@10 mb" --barbs "UGRD@10 mb" "VGRD@10 mb" --id geos_10mb_wind
   python3 tools/make_chart.py IFS "TMP@2 m above ground" --catalog catalog.json       (checks the fields are in the model's files)

FILL is a field "VARIABLE@level", or speed:U,V (the speed of two fields), diff:A,B (A minus B), mean:A,B. The ramp, the units
(scale, offset, quantity) and the legend step are chosen from the variable where it is known; pass --ramp to choose another
(names: see chart-files.md). Put the file in your models folder (Settings > Model Sources > "Your file...") and restart.
The output is a starting point: check the units of the field (the grids are in the model's own, degrees C for temperature,
Pa for pressure ...) against the ramp's, and the title.
"""
import argparse, json, re, sys

# variable (regex on the NOAA name) -> fill defaults; the scale turns the grid's unit into the ramp's
FILL = [
    (r'^(TMP|TMAX|TMIN|APTMP)$', dict(ramp='temperature', quantity='temperature', legend_step=5, title='Temperature')),
    (r'^DPT$', dict(ramp='dewpoint', quantity='temperature', legend_step=5, title='Dewpoint')),
    (r'^(RH|MINRH|MAXRH)$', dict(ramp='humidity', legend_step=10, title='Relative humidity (%)')),
    (r'^PWAT$', dict(ramp='precipitable-water', quantity='millimeters', legend_step=10, title='Precipitable water')),
    (r'^(APCP|ACPCP|NCPCP)$', dict(ramp='precipitation', quantity='millimeters', title='Precipitation')),
    (r'^CAPE$', dict(ramp='cape', legend_step=1000, title='CAPE (J/kg)')),
    (r'^(TCDC|LCDC|MCDC|HCDC)$', dict(ramp='cloud-cover', legend_step=10, title='Cloud cover (%)')),
    (r'^(REFC|REFD|MAXREF)$', dict(ramp='reflectivity', legend_step=10, title='Reflectivity (dBZ)')),
    (r'^(GUST|WIND)$', dict(ramp='wind', scale=1.94384, legend_step=20, title='Wind (kt)')),
    (r'^(PRMSL|MSLET|MSLMA|PRES)$', dict(ramp='sea-level-pressure', scale=0.01, legend_step=10, title='Pressure (mb)')),
    (r'^HLCY$', dict(ramp='helicity', title='Storm-relative helicity (m2/s2)')),
    (r'^(MNUPHL|MXUPHL|UPHL)$', dict(ramp='updraft-helicity', title='Updraft helicity (m2/s2)')),
    (r'^(ABSV|RELV)$', dict(ramp='vorticity', scale=100000, legend_step=10, title='Vorticity (1e-5 /s)')),
    (r'^(WEASD|SNOD)$', dict(ramp='snowfall', scale=100, title='Snow (cm)')),
    (r'^SOILW$', dict(ramp='humidity', scale=100, title='Soil moisture (%)')),
    (r'^TSOIL$', dict(ramp='temperature', quantity='temperature', legend_step=5, title='Soil temperature')),
    (r'^(HTSGW|WVHGT|SWELL)$', dict(ramp='wave-height', title='Wave height (m)')),
    (r'^WTMP$', dict(ramp='sea-surface-temperature', quantity='temperature', title='Sea surface temperature')),
    (r'^VIS$', dict(ramp='visibility', scale=0.000621371, title='Visibility (mi)')),
]
CONTOUR = [
    (r'^HGT$', dict(scale=0.1, interval=6, title='Height (dam)')),
    (r'^(PRMSL|MSLET|MSLMA|PRES)$', dict(scale=0.01, interval=4, title='Pressure (mb)', highs_lows=True)),
    (r'^(TMP|DPT)$', dict(scale=1, interval=5, title='Temperature (C)', dashed=True, color='#c0392b')),
]

def split_field(text):
    if '@' not in text:
        sys.exit(f'a field is VARIABLE@level, not "{text}"')
    return text.split('@', 1)[0].strip()

def pick(table, variable):
    for pattern, spec in table:
        if re.match(pattern, variable):
            return dict(spec)
    return None

ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
ap.add_argument('model')
ap.add_argument('fill', help='VARIABLE@level, or speed:U,V  diff:A,B  mean:A,B; "none" for lines only')
ap.add_argument('--contour', action='append', default=[], metavar='FIELD')
ap.add_argument('--barbs', nargs=2, metavar=('U', 'V'))
ap.add_argument('--id'); ap.add_argument('--label'); ap.add_argument('--ramp'); ap.add_argument('--title')
ap.add_argument('--interval', type=float, help='the interval of the first contour')
ap.add_argument('--catalog', help='the check\'s catalog.json: the fields are looked for in the model\'s files')
a = ap.parse_args()

chart = {'id': None, 'model': a.model, 'label': None, 'group': 'Your charts'}
first = None
notes = []
if a.fill.lower() != 'none':
    m = re.match(r'^(speed|diff|mean):(.+),(.+)$', a.fill)
    if m:
        kind, f1, f2 = m.group(1), m.group(2).strip(), m.group(3).strip()
        fill = {kind: [f1, f2]}
        v = split_field(f1)
        spec = pick(FILL, 'WIND' if kind == 'speed' else v) or {}
        first = f1
        fields = [f1, f2]
    else:
        fill = {'field': a.fill}
        v = split_field(a.fill)
        spec = pick(FILL, v) or {}
        first = a.fill
        fields = [a.fill]
    if not spec:
        notes.append(f'no ramp is known for {split_field(first)}: pass --ramp (and check the units)')
        spec = {'ramp': 'probability'}
    if a.ramp:
        spec['ramp'] = a.ramp
    if a.title:
        spec['title'] = a.title
    fill.update(spec)
    chart['fill'] = fill
else:
    fields = []
if a.contour:
    chart['contours'] = []
    for i, f in enumerate(a.contour):
        spec = pick(CONTOUR, split_field(f)) or dict(scale=1, interval=1, title=f)
        if split_field(f) not in ('HGT', 'PRMSL', 'MSLET', 'MSLMA', 'PRES', 'TMP', 'DPT'):
            notes.append(f'contour {f}: set the scale and interval for its units')
        if i == 0 and a.interval:
            spec['interval'] = a.interval
        chart['contours'].append(dict(field=f, **spec))
        fields.append(f)
if a.barbs:
    chart['barbs'] = {'u': a.barbs[0], 'v': a.barbs[1]}
    fields += list(a.barbs)
if not chart.get('fill') and not chart.get('contours') and not chart.get('barbs'):
    sys.exit('a chart has a fill, lines or barbs')
name = re.sub(r'[^A-Za-z0-9_-]+', '_', (a.id or (first or a.contour[0]) .replace('@', '_')).strip('_')).lower()
chart['id'] = name
chart['label'] = a.label or (', '.join(dict.fromkeys(split_field(f) + '@' + f.split('@', 1)[1].strip() for f in fields if '@' in f)))
if a.catalog:
    data = {m['model']: m for m in json.load(open(a.catalog))}
    m = data.get(a.model)
    if not m or not m['checked']:
        notes.append(f'{a.model} was not checked in {a.catalog}: the fields could not be looked for')
    else:
        listed = set()
        for f in m['found'] + m['not_cataloged']:
            listed.add((f['variable'], f.get('level', '')))
        for f in fields:
            var, level = f.split('@', 1)[0].strip(), f.split('@', 1)[1].split('|')[0].strip()
            if (var, level) not in listed:
                notes.append(f'{f}: the model\'s newest file does not list it (the listed levels of {var}: ' + ', '.join(sorted(l for v, l in listed if v == var))[:120] + ')')
print(json.dumps({'version': 1, 'charts': [chart]}, indent=2))
for n in notes:
    print('note:', n, file=sys.stderr)
