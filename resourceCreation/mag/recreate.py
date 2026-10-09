import json, collections
inv = json.load(open('mag_inventory.json'))
cap = json.load(open('captions.json'))
drawn = collections.defaultdict(set)
for l in open('/tmp/claude-1000/prod/drawn_now.tsv'):
    p = l.rstrip('\n').split('\t')
    if len(p) >= 3: drawn[p[0]].add(p[1])
# the open data source each MAG model is recreated from (nothing is retired: the old screens stay until a map is recreated and checked)
source = {'GFS-WAVE': 'GFS-WAVE', 'GEFS-WAVE': 'GEFS-WAVE', 'GEFS-SPAG': 'GEFS', 'GFS': 'GFS', 'AIGFS': 'AIGFS', 'NBM': 'NBM', 'GEFS-MEAN-SPRD': 'GEFS', 'NAM': 'RRFS', 'NAM-HIRES': 'RRFS', 'HRRR': 'RRFS', 'RAP': 'RRFS', 'FIREWX': 'RRFS', 'HRW-FV3': 'RRFS', 'HRW-ARW': 'RRFS', 'HRW-ARW2': 'RRFS',
          'HREF': 'HREF', 'SREF': 'REFS', 'SREF-CLUSTER': 'REFS'}
# maps whose id differs but that are the same chart (HREF / SREF -> the REFS chart of the same thing)
alias = {('HREF', 'mean_precip_p01'): 'mean_precip_p01', ('HREF', 'mean_precip_p03'): 'mean_precip_p03', ('HREF', 'prob_refd_40dbz'): 'prob_refc_40', ('HREF', 'prob_cref_40dbz'): 'prob_refc_40',
         ('HREF', 'prob_cref_50dbz'): 'prob_refc_50', ('HREF', 'prob_3h_rain_0.5in'): 'prob_precip_3h_0.5in', ('HREF', 'prob_3h_rain_1in'): 'prob_precip_3h_1in', ('HREF', 'eas_prob_1h_rain_0.25in'): 'prob_precip_1h_0.25in',
         ('HREF', 'eas_prob_1h_rain_0.5in'): 'prob_precip_1h_0.5in', ('HREF', 'eas_prob_3h_rain_0.5in'): 'prob_precip_3h_0.5in', ('HREF', 'prob_3h_snow_1in'): 'prob_snow_3h_1in', ('HREF', 'prob_3h_snow_3in'): 'prob_snow_3h_3in',
         ('HREF', 'mean_2m_temp'): 'mean_2m_temp', ('HREF', 'max_updraft_hlcy'): 'max_uphl', ('SREF', 'precip_p03'): 'mean_precip_p03', ('SREF', 'mean_2m_temp'): 'mean_2m_temp', ('RAP', 'cape_cin'): 'sfc_cape_cin', ('NBM', '6hour_accu_snow'): 'snow_p06', ('NBM', 'total_accu_snow'): 'snow_ptot'}
covered = {'SREF', 'SREF-CLUSTER', 'NAEFS'}
last = set()   # to be recreated after everything else (the user's order)   # the usual operational ensembles: the SPC REFS viewer (and the older ensemble screens) already cover them
rows = []
for model, v in inv.items():
    for p in v['products']:
        pid = p['id']; src = source.get(model)
        if model == 'STORM-TRACKS':
            status, by = ('to recreate', 'the strike-probability map from the GEFS member tracks (ATCF), not drawn yet') if pid == 'GEFS-prob' else ('covered by the hurricane screens', 'the model tracks (ATCF guidance) the hurricane screens draw')
        elif model == 'STOFS':
            status, by = 'to recreate', 'STOFS is NetCDF on an unstructured grid (not GRIB): its own project'
        elif model == 'ICE-DRIFT':
            status, by = 'to recreate', 'no open GRIB source found for the polar ice drift (the old polar.ncep.noaa.gov path is gone)'
        elif model in covered:
            status, by = 'covered by the SPC REFS viewer', 'existing screens'
        elif model in last and not ((model, pid) in alias and alias[(model, pid)] in drawn[src or '']):
            status, by = 'to recreate (last)', 'REFS members / ready-made products, after everything else'
        elif src is None:
            status, by = 'to recreate', '(no source chosen yet)'
        elif pid in drawn[src]:
            status, by = 'recreated', f'{src} `{pid}`'
        elif (model, pid) in alias and alias[(model, pid)] in drawn[src]:
            status, by = 'recreated (equivalent)', f'{src} `{alias[(model, pid)]}`'
        elif (model, pid) == ('NBM', 'precip_duration'):
            status, by = 'to recreate', 'NBM: no precipitation duration record in the blend files (checked); would need deriving from the hourly amounts'
        else:
            status, by = 'to recreate', f'{src}'
        rows.append((model, pid, p['label'], status, by, f'{model}/{pid}' in cap))
summary = collections.OrderedDict()
for r in rows:
    summary.setdefault(r[0], collections.Counter())[r[3]] += 1
tot = collections.Counter(r[3] for r in rows)
out = ['# The maps to recreate', '',
       'Every product the model guidance site (MAG) offers, 566 maps in 23 models: the list to work down. Nothing is retired or replaced while this is worked: the older screens and readers stay until a map is recreated and checked. `recreated` = the same map is drawn from GRIB by us (the chart named); `recreated (equivalent)` = the same thing drawn by a chart with another name; `to recreate` = still to draw (the source it will be drawn from is named; the layers it needs are in `docs/mag-composition.md`, the picture in `docs/mag-reference/`).', '',
       f"**Now: {tot['recreated']} recreated, {tot['recreated (equivalent)']} recreated (equivalent), {tot['to recreate']} to recreate now ({tot['to recreate (last)']} HREF maps more, last), and {tot['covered by the SPC REFS viewer']} (SREF, SREF-CLUSTER, NAEFS) left to the SPC REFS viewer and the existing ensemble screens. The HREF maps are done last.**", '',
       '| MAG model | maps | recreated | to recreate | drawn from |', '|---|---|---|---|---|']
for m, c in summary.items():
    n = sum(c.values()); done = c['recreated'] + c['recreated (equivalent)']
    out.append(f"| {m} | {n} | {done} | {c['to recreate'] + c['to recreate (last)']} | {'the SPC REFS viewer (existing)' if m in covered else 'RRFS 3 km (SPC fire weather screens also cover part of the fire weather set)' if m == 'FIREWX' else source.get(m, 'not chosen yet')} |")
out += ['']
cur = None
for r in rows:
    if r[0] != cur:
        cur = r[0]
        out += ['', f'## {cur}', '', '| map | label | status | by |', '|---|---|---|---|']
    out.append(f"| `{r[1]}` | {r[2]} | {r[3]} | {r[4]} |")
open('/home/mitch/Claude/wxqt-grib/docs/maps-to-recreate.md', 'w').write('\n'.join(out) + '\n')
print(dict(tot))
for m, c in summary.items(): print(f"{m:16s} {sum(c.values()):3d} maps  {c['recreated']+c['recreated (equivalent)']:3d} recreated  {c['to recreate']:3d} to do")
json.dump(rows, open('/tmp/claude-1000/mag/recreate.json', 'w'))
