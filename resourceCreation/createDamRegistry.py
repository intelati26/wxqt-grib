#!/usr/bin/env python3
"""Builds resourceCreation/res/dams_usace.txt: the Corps of Engineers hydropower projects whose hourly data the Corps' public CWMS Data API
(cwms-data.usace.army.mil/cwms-data) serves for the Little Rock (SWL) and Tulsa (SWT) districts, with their positions and the time series to read.

One project a line, tab separated:
  office, project id, public name, latitude, longitude (east positive), nearest city, state,
  pool elevation series, tailwater elevation series, total outflow series, turbine flow series, inflow series, generation series (several joined by ';')
A missing series is '-'. Series are chosen by rule from the catalog: observed (not forecast, not raw) hourly series, the best version first.
Usage: createDamRegistry.py   (needs the network; stdlib only)
"""
import json, re, sys, time, urllib.parse, urllib.request

BASE = 'https://cwms-data.usace.army.mil/cwms-data'
OFFICES = ('SWL', 'SWT')
VERSIONS = ['Decodes-rev', 'Rev-SCADA', 'Rev-Regi-Flowgroup', 'Ccp-Rev', 'CCP-Comp', 'Rev-Regi-Computed', 'Rev']

def get(path, params, version='2', tries=4):
    url = BASE + path + '?' + urllib.parse.urlencode(params)
    for attempt in range(tries):
        try:
            request = urllib.request.Request(url, headers={'Accept': 'application/json;version=' + version, 'User-Agent': 'wxqt registry builder'})
            return json.load(urllib.request.urlopen(request, timeout=60))
        except Exception as error:
            last = error
            time.sleep(1.5)
    raise last

def rank(name):
    version = name.split('.')[-1]
    if 'Forecast' in version or 'raw' in version.lower() or 'Interlaced' in version or 'Crest' in version:
        return None
    for i, v in enumerate(VERSIONS):
        if version.lower() == v.lower():
            return i
    return len(VERSIONS)

def best(names, pattern, exclude=None):
    candidates = []
    for name in names:
        if re.search(pattern, name) and not (exclude and re.search(exclude, name)):
            r = rank(name)
            if r is not None:
                candidates.append((r, name))
    return sorted(candidates)[0][1] if candidates else None

def main():
    lines = []
    for office in OFFICES:
        catalog = get('/catalog/TIMESERIES', {'office': office, 'like': '.*Energy-Gen.*', 'page-size': 1000})
        ids = sorted(set(re.split(r'[.-]', e['name'])[0] for e in catalog['entries']))
        for pid in ids:
            names = [e['name'] for e in get('/catalog/TIMESERIES', {'office': office, 'like': '^' + re.escape(pid) + '[.-].*', 'page-size': 1000})['entries'] if '1Hour' in e['name']]
            pool = best(names, r'(Headwater\.Elev|\.Elev)\.Inst\.1Hour', r'Tailwater|Elev-')
            tail = best(names, r'(Tailwater\.Elev-Downstream|Elev-Tailwater)\.Inst\.1Hour')
            out = best(names, r'Tailwater\.Flow\.Inst\.1Hour') or best(names, r'Flow-Res Out\.Inst\.1Hour') or best(names, r'Flow-Out\.Inst\.1Hour')
            power = best(names, r'\.Flow-Plant\.Ave\.1Hour') or best(names, r'Flow-Power\.(Ave|Inst)\.1Hour')
            inflow = best(names, r'Flow-Res In\.Ave\.1Hour') or best(names, r'Flow-In\.')
            # generation: the project's own total when there is one (House_Unit), else every unit's total
            plant = best(names, r'\.Energy-Gen_Plant\.Total\.1Hour')   # the plant's own total (what the Corps' district pages show), when there is one
            gens = {}
            for name in names:
                m = re.match(r'^(.*?)\.Energy-Gen\.Total\.1Hour\.1Hour\.(.*)$', name)
                if m and rank(name) is not None:
                    gens.setdefault(m.group(1), []).append(name)
            chosen = []
            if plant:
                chosen = [plant]
            elif gens:
                whole = [loc for loc in gens if re.fullmatch(re.escape(pid) + r'-House_Unit|' + re.escape(pid), loc)]
                for loc in (whole or sorted(gens)):
                    chosen.append(sorted(gens[loc], key=rank)[0])
            if not chosen or not (out or power):
                print('skip', office, pid, 'generation', len(chosen), 'outflow', out, file=sys.stderr)
                continue
            # the position: the project's own, else its first unit's, else the published one (longitude given without a sign for the western hemisphere)
            info = get('/locations/' + pid, {'office': office})
            lat, lon = info.get('latitude'), info.get('longitude')
            if lat is None:
                for loc in sorted(gens):
                    sub = get('/locations/' + loc, {'office': office})
                    if sub.get('latitude') is not None:
                        lat, lon = sub['latitude'], sub['longitude']
                        break
            if lat is None and info.get('published-latitude') is not None:
                lat, lon = info['published-latitude'], -abs(info['published-longitude'])
            if lat is None:
                print('skip', office, pid, 'no position', file=sys.stderr)
                continue
            # a sanity check: the largest hourly generation over the last two weeks (an idle feed shows zero all the time)
            def peak_of(series_names):
                total = 0.0
                for series_name in series_names:
                    end = time.gmtime()
                    begin = time.gmtime(time.time() - 14 * 86400)
                    data = get('/timeseries', {'office': office, 'name': series_name.replace('mw:', ''), 'begin': time.strftime('%Y-%m-%dT%H:%M:%SZ', begin), 'end': time.strftime('%Y-%m-%dT%H:%M:%SZ', end), 'unit': 'EN', 'page-size': 2000})
                    total += max([v[1] for v in data.get('values', []) if v[1] is not None] or [0.0])
                return total
            peak = peak_of(chosen)
            if peak == 0.0:
                # the Tulsa district publishes no hourly energy: its turbines report power (MW) every 15 minutes, which the program averages by the hour ("mw:" marks them)
                power_gen = {}
                for name in get('/catalog/TIMESERIES', {'office': office, 'like': '^' + re.escape(pid) + r'[.-].*Power-Gen\.Inst\.15Minutes.*', 'page-size': 200})['entries']:
                    power_gen.setdefault(name['name'].split('.')[0], []).append(name['name'])
                alt = ['mw:' + sorted(v, key=rank)[0] for k, v in sorted(power_gen.items()) if rank(sorted(v, key=rank)[0]) is not None]
                alt_peak = peak_of(alt) if alt else 0.0
                if alt_peak > 0.0:
                    chosen, peak = alt, alt_peak
            city = (info.get('nearest-city') or '').split(',')[0]
            row = [office, pid, info.get('public-name') or pid, '%.5f' % lat, '%.5f' % lon, city, info.get('state-initial') or '',
                   pool or '-', tail or '-', out or '-', power or '-', inflow or '-', ';'.join(chosen)]
            lines.append('\t'.join(row))
            print('ok', office, pid, row[2], row[3], row[4], 'peak MWh/h in 14 days: %.1f' % peak, file=sys.stderr)
    open('resourceCreation/res/dams_usace.txt', 'w').write('\n'.join(lines) + '\n')
    print(len(lines), 'projects')

main()
