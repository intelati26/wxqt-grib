#!/usr/bin/env python3
"""Turns the catalogue check's JSON (WXQT_OPEN=catalogcheck:*:catalog.json, or the daily workflow's artifact) into files to read and research from:

   catalog-fields.csv       every field of every checked model: used / not used (never found, gone), the charts that use it, its family
   catalog-not-used.csv     only the fields the files list and no chart asks for
   catalog-families.csv     family x model: "used/not used" counts of distinct fields
   catalog-by-variable.csv  family, variable x model: U<n> (n levels used) N<n> (n levels listed, not used); the models' own names for it
   catalog-report.md        the tables above in words: which models have which, by family

   python3 tools/catalog_report.py catalog.json [out-folder]

The models name the same thing differently (NOAA's GRIB names, Open-Meteo's `temperature_500hPa`, a NetCDF's `T500`): a name is reduced to a
common variable (`TMP`) and a level, and the common variable to a family by the patterns below. A GRIB parameter the index gives only as a code
(`var discipline=0 ... parmcat=1 parm=196`) is its own family, "Unnamed GRIB parameters", with the code kept: those are the ones to look up.
"""
import csv, json, os, re, sys
from collections import defaultdict

# Open-Meteo's names (a level may be in the name) -> the NOAA name the program uses
OM = {'temperature': 'TMP', 'relative_humidity': 'RH', 'geopotential_height': 'HGT', 'wind_u_component': 'UGRD', 'wind_v_component': 'VGRD', 'vertical_velocity': 'VVEL',
      'cloud_cover': 'TCDC', 'cloud_cover_low': 'LCDC', 'cloud_cover_mid': 'MCDC', 'cloud_cover_high': 'HCDC', 'precipitation': 'APCP', 'wind_gusts': 'GUST',
      'shortwave_radiation': 'DSWRF', 'direct_radiation': 'DSWRF (direct)', 'convective_inhibition': 'CIN', 'snow_depth': 'SNOD', 'wind_speed': 'WIND', 'wind_direction': 'WDIR',
      'surface_temperature': 'TMP (surface)', 'dew_point': 'DPT', 'pressure_msl': 'PRMSL', 'surface_pressure': 'PRES', 'cape': 'CAPE', 'visibility': 'VIS', 'weather_code': 'weather code',
      'freezing_level_height': 'freezing level', 'soil_temperature': 'TSOIL', 'soil_moisture': 'SOILW', 'rain': 'rain', 'showers': 'showers', 'snowfall_water_equivalent': 'snowfall (water)'}
LEVEL = re.compile(r'^(.*?)_(\d+(?:_to_\d+)?(?:hPa|m|cm|mm))$')

# first match wins
FAMILIES = [
    ('Unnamed GRIB parameters', r'^var(\b|\d|\s)'),
    ('Ensemble spread', r'.*_spread$'),
    ('Grid and geometry', r'^(latitude|longitude|cosalpha|sinalpha|NLAT|ELON|critical_angle|LMH|LMV)$'),
    ('Satellite / brightness temperature', r'^(SBT|SBTA|BRTEMP|CSDSF)'),
    ('Waves and ocean', r'^(HTSGW|SWELL|SWPER|SWDIR|WVDIR|WVHGT|WVPER|DIRPW|PERPW|5WAV|WTMP|UOGRD|VOGRD|WWSDIR|MWSPER|FRZSPR)'),
    ('Air quality and aerosol', r'^(MASSDEN|AOTK|AOD|COLMD|PM25|PM10|PM|O3|TO3|TOX|TOZNE|AEMFLX|BC|BR|DU|SS|SU|OC|NI|NH4|NH3|DMS|SO2|SO4|HNO3|CO$|NO|PAN|OH|HCHO|GLYX|SALT|DUST|TOT(ANGSTR|EXTTAU|SCATAU|ST)|JFWPRB|IMWF)'),
    ('Radar and lightning', r'^(REFC|REFD|MAXREF|RETOP|VIL|LTNG|LTNGSD|ECHOTOP|lightning|THUNC|TSTM)'),
    ('Fire weather', r'^(fosberg|pyrocb|FIREDLA|vpd|HINDEX)'),
    ('Convection and severe weather', r'^(CAPE|CIN|DCAPE|LFTX|4LFTX|PLI|HLCY|MNUPHL|MXUPHL|UPHL|EFHL|MAXUVV|MAXDVV|HAIL|CWORK|VUCSH|VVCSH|VWSH|STP|SHIP|convective|ml_|mu_|sb_|srh|shear_|srw_|stp|scp|ehi_|lapse_rate|dcp|dgz|hgz|eil_|mcs_|supercell|mean_wind|effective_shear|lhp|shp|snow_squall|dcape|dcinh|lifted_index|ELLINX)'),
    ('Boundary layer and tropopause', r'^(PBLTOP|BMIXL|MIXHT|DISPH|DIST|hght_pbl|TURB|TROP|HPBL|PLPL)'),
    ('Wind', r'^(UGRD|VGRD|WIND|WDIR|GUST|USTM|VSTM|MAXUW|MAXVW|UFLX|VFLX|FRICV|U-GWD|V-GWD|CD$|UESH|VESH|UEID|VEID|U2M|V2M|U50M|V50M|uwin|vwin|TRW)'),
    ('Humidity and moisture', r'^(RH|SPFH|DPT|DEPR|PWAT|pwat|MINRH|MAXRH|RHPW|CWAT|CLWMR|CLMR|RWMR|SNMR|ICMR|GRLE|TCOLW|TCOLI|CIMIXR|MCONV|CICE|Q250|QV|QI|QL|QR|QS|TQ|TCOLG|mixr|relh|total_column|dwpk|WCCONV|WVCONV|WCINC|WVINC|WCUFLX|WVUFLX|WCVFLX|WVVFLX)'),
    ('Precipitation', r'^(APCP|ACPCP|NCPCP|PRATE|CPRAT|CRAIN|CSNOW|CFRZR|CICEP|CPOFP|PTYPE|ASNOW|FROZR|FRZR|TSNOWP|WATR|SSRUN|BGRUN|PEVPR|PEVAP|FICEAC|PWTHER|rain|showers|weather code|snowfall|precipitation_type)'),
    ('Cloud', r'^(TCDC|LCDC|MCDC|HCDC|CDCB|CEIL|CWASP|ICI|CFNSF|CDCON|CDCTOP|CDLYR|CLDPRS|CLDTMP)'),
    ('Radiation and energy', r'^(DSWRF|USWRF|DLWRF|ULWRF|VBDSF|VDDSF|SUNSD|ALBDO|LHTFL|SHTFL|GFLUX|SNOHF|SFEXC|UVI|diffuse_radiation|shortwave_radiation|latent_heat_flux|sensible_heat_flux|direct)'),
    ('Pressure and height', r'^(PRES|PRMSL|MSLET|MSLMA|HGT|HGTMD|GPA|PS$|SLP|pres_|PHIS|hght|H1000|GP|IBHG)'),
    ('Vorticity and dynamics', r'^(ABSV|RELV|STRM|VPOT|EPV|VVEL|DZDT|OMEGA|TKE|VRATE|CAT|PTEND)'),
    ('Snow and ice', r'^(WEASD|SNOD|SNOWC|SNOM|SDEN|SBSNO|ICEC|ICETK|ICEG|SNOWLVL|SNOWLR|freezing level|fzl_|ICAHT|ICETMP|snow_depth)'),
    ('Land and soil', r'^(SOILW|SOILL|TSOIL|SOILM|SOTYP|VGTYP|VEG|LAI|WILT|POROS|RSMIN|FLDCP|SMDRY|SMREF|MSTAV|CNWAT|SFCR|LAND|RLYRS|CANGLE|LAYTH|ELMELT|MXSALB|CISOILM|CCOND|RC|EPOT|EVP|ESP)'),
    ('Visibility and aviation', r'^(VIS|FLGHT)'),
    ('Temperature', r'^(TMP|TMAX|TMIN|APTMP|POT|surface|TSOIL|T10M|T250|TS$|tmpk|tmwk|theta|thetae|temperature)'),
]

def reduce(name):
    """-> (common variable, level text from the name, family)"""
    level = ''
    base = name
    m = LEVEL.match(name)
    if m and (m.group(1) in OM or m.group(1) in ('wind_speed', 'wind_direction', 'temperature', 'wind_u_component', 'wind_v_component')):
        base, level = m.group(1), m.group(2)
    common = OM.get(base, base)
    if re.match(r'^var(\b|\d|\s)', name):
        code = re.sub(r'^var\s*', '', name)
        common = 'code: ' + (code if code else name)
    for fam, pattern in FAMILIES:
        if re.match(pattern, common) or re.match(pattern, name):
            return common, level, fam
    if base in OM or name in OM:
        return common, level, 'Other'
    return common, level, 'Other'

src = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(os.path.abspath(src))
os.makedirs(out, exist_ok=True)
data = [m for m in json.load(open(src))]
checked = sorted([m for m in data if m['checked']], key=lambda m: m['model'])
models = [m['model'] for m in checked]

rows = []   # model, run, status, family, common, own name, level, detail, file, charts_using, chart_examples
def add(m, status, f):
    common, lvl, fam = reduce(f['variable'])
    rows.append([m['model'], m['run'], status, fam, common, f['variable'], f.get('level') or lvl, f.get('detail', ''), f.get('file', ''), f.get('used_by_count', 0), ' '.join(f.get('used_by', []))])
for m in checked:
    for f in m['found']: add(m, 'used', f)
    for f in m['gone']: add(m, 'gone', f)
    for f in m['never_found']: add(m, 'never found', f)
    for f in m['not_cataloged']: add(m, 'not used', f)

head = ['model', 'run', 'status', 'family', 'variable', 'name in the file', 'level', 'detail', 'file', 'charts_using', 'chart_examples']
with open(os.path.join(out, 'catalog-fields.csv'), 'w', newline='') as a, open(os.path.join(out, 'catalog-not-used.csv'), 'w', newline='') as b:
    wa, wb = csv.writer(a), csv.writer(b)
    wa.writerow(head); wb.writerow(head[:9])
    for r in sorted(rows, key=lambda r: (r[3], r[4], r[0], r[6])):
        wa.writerow(r)
        if r[2] == 'not used':
            wb.writerow(r[:9])

# family x model
fam_counts = defaultdict(lambda: defaultdict(lambda: [0, 0]))   # family -> model -> [used, not used]
for r in rows:
    key = (r[4], r[6], r[7], r[8])
    if r[2] == 'used':
        fam_counts[r[3]][r[0]][0] += 1
    elif r[2] == 'not used':
        fam_counts[r[3]][r[0]][1] += 1
families = sorted(fam_counts, key=lambda f: (f == 'Other', f == 'Unnamed GRIB parameters', f))
with open(os.path.join(out, 'catalog-families.csv'), 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['family'] + models)
    for fam in families:
        w.writerow([fam] + [('%d/%d' % tuple(fam_counts[fam][mod])) if mod in fam_counts[fam] else '' for mod in models])

# variable x model: levels used / listed only
var = defaultdict(lambda: defaultdict(lambda: [set(), set()]))   # (family, variable) -> model -> [levels used, levels not used]
names = defaultdict(set)
for r in rows:
    if r[2] not in ('used', 'not used'):
        continue
    k = (r[3], r[4])
    var[k][r[0]][0 if r[2] == 'used' else 1].add(r[6] or '-')
    names[k].add(r[5])
with open(os.path.join(out, 'catalog-by-variable.csv'), 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['family', 'variable', 'names in the files', 'models listing it', 'models using it'] + models)
    for k in sorted(var, key=lambda k: (k[0] == 'Other', k[0] == 'Unnamed GRIB parameters', k[0], k[1])):
        cells = []
        for mod in models:
            u, n = var[k].get(mod, [set(), set()])
            cells.append((('U%d' % len(u)) if u else '') + (('N%d' % len(n)) if n else '') if (u or n) else '')
        w.writerow([k[0], k[1], '; '.join(sorted(names[k]))[:120], len(var[k]), sum(1 for mod in var[k] if var[k][mod][0])] + cells)

# the report
L = ['# Catalogue of fields: which models have what', '',
     f'{len(checked)} models checked of {len(data)} (the rest have no index or variable list to read: Zarr stores, tar files, local runs, files with no index, or no run on the server). '
     '**Used**: a chart asks for it and the files have it. **Not used**: the files list it and no chart asks for it (the list to research). Names are reduced to a common variable and a level, then to a family; '
     'the first table is the number of distinct fields (variable, level, detail) a model lists, as `used / not used`.', '']
short = {m: m for m in models}
for chunk in range(0, len(models), 9):
    cols = models[chunk:chunk + 9]
    L += ['| family | ' + ' | '.join(cols) + ' |', '|---|' + '---|' * len(cols)]
    for fam in families:
        L.append('| ' + fam + ' | ' + ' | '.join(('%d / %d' % tuple(fam_counts[fam][mod])) if mod in fam_counts[fam] else '' for mod in cols) + ' |')
    L.append('')
L += ['## By family: which models have which variable', '']
for fam in families:
    ks = sorted([k for k in var if k[0] == fam], key=lambda k: (-len(var[k]), k[1]))
    if not ks:
        continue
    L += [f'### {fam}', '']
    for k in ks:
        used = sorted(mod for mod in var[k] if var[k][mod][0])
        only = sorted(mod for mod in var[k] if not var[k][mod][0])
        alias = sorted(names[k] - {k[1]})
        line = f'- **`{k[1]}`**' + (f' (also named {", ".join("`%s`" % a for a in alias[:4])}{"..." if len(alias) > 4 else ""})' if alias else '')
        line += f': {len(var[k])} models'
        if used:
            line += f'. Used by: {", ".join(used)}'
        if only:
            line += f'. Listed only: {", ".join(only[:25])}{"..." if len(only) > 25 else ""}'
        L.append(line)
    L.append('')
open(os.path.join(out, 'catalog-report.md'), 'w').write('\n'.join(L) + '\n')
print('written to', out, '|', len(rows), 'rows,', len(families), 'families,', len(var), 'variables')
