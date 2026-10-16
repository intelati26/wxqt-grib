#!/usr/bin/env python3
# resourceCreation/createHighRiskDays.py: writes res/spc_high_risk_days.txt (run from this folder; about 15 minutes, 300 requests of 2 MB; add the newest months by running it again)
# Day 1 convective outlooks with a HIGH risk, from the IEM archive (one request per month): the convective day, the issuances with HIGH,
# and the extent of the HIGH area (the biggest polygon of the day). Output: spc_high_risk_days.txt
import io, struct, sys, zipfile, urllib.request, concurrent.futures, calendar, math, collections

def month_windows():
    for y in range(2002, 2027):
        for m in range(1, 13):
            if y == 2026 and m > 10: continue
            ny, nm = (y + 1, 1) if m == 12 else (y, m + 1)
            yield y, m, ny, nm

def fetch(args):
    y, m, ny, nm = args
    url = ("https://mesonet.agron.iastate.edu/cgi-bin/request/gis/spc_outlooks.py?d=1&type=C"
           f"&year1={y}&month1={m}&day1=1&hour1=0&minute1=0&year2={ny}&month2={nm}&day2=1&hour2=0&minute2=0")
    for attempt in range(3):
        try:
            data = urllib.request.urlopen(url, timeout=240).read()
            return args, data
        except Exception as e:
            err = e
    return args, None

def parse(data):
    z = zipfile.ZipFile(io.BytesIO(data))
    names = z.namelist()
    dbf = z.read([n for n in names if n.endswith('.dbf')][0])
    shp = z.read([n for n in names if n.endswith('.shp')][0])
    shx = z.read([n for n in names if n.endswith('.shx')][0])
    n = struct.unpack('<I', dbf[4:8])[0]; hl = struct.unpack('<H', dbf[8:10])[0]; rl = struct.unpack('<H', dbf[10:12])[0]
    fields = []; i = 32
    while dbf[i] != 0x0D:
        fields.append((dbf[i:i+11].split(b'\0')[0].decode(), dbf[i+16])); i += 32
    out = []
    for r in range(n):
        rec = dbf[hl + r*rl: hl + (r+1)*rl]; p = 1; d = {}
        for name, ln in fields:
            d[name] = rec[p:p+ln].decode('latin1').strip(); p += ln
        if d.get('THRESHOLD') != 'HIGH' or d.get('CATEGORY') != 'CATEGORICAL':
            continue
        off = struct.unpack('>I', shx[100 + 8*r: 104 + 8*r])[0] * 2
        # record: 8 byte header, then shape type, bbox(4 doubles)
        st = struct.unpack('<i', shp[off+8:off+12])[0]
        if st == 0: continue
        xmin, ymin, xmax, ymax = struct.unpack('<4d', shp[off+12:off+44])
        d['bbox'] = (xmin, ymin, xmax, ymax)
        out.append(d)
    return out

def main():
    days = collections.defaultdict(list)
    with concurrent.futures.ThreadPoolExecutor(4) as pool:
        for args, data in pool.map(fetch, list(month_windows())):
            if data is None:
                print('FAILED', args, file=sys.stderr); continue
            try:
                rows = parse(data)
            except Exception as e:
                print('PARSE', args, e, file=sys.stderr); continue
            for d in rows:
                days[d['EXPIRE'][:8]].append(d)   # an outlook expires at 12Z of the end of its convective day
            print(args, len(rows), flush=True)
    with open("res/spc_high_risk_days.txt", "w") as f:
        f.write('# SPC Day 1 categorical outlooks with a HIGH risk (IEM archive, 2002 on): convective day (12Z to 12Z, the date it ends), first issuance with HIGH, issuances with it, bbox of the biggest HIGH area\n')
        f.write('# date_ending first_issue count west south east north\n')
        for day in sorted(days):
            rows = days[day]
            issues = sorted({r['ISSUE'] for r in rows})
            big = max(rows, key=lambda r: (r['bbox'][2]-r['bbox'][0])*(r['bbox'][3]-r['bbox'][1]))
            w, s, e, n = big['bbox']
            f.write(f"{day} {issues[0]} {len(issues)} {w:.2f} {s:.2f} {e:.2f} {n:.2f}\n")
    print('days', len(days))
main()
