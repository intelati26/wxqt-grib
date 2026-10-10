import json, os, re, sys, time, urllib.request, urllib.parse
UA = "wxqt-reference (one-off set of reference images of the model guidance maps; will not repeat)"
OUT = "/home/mitch/Claude/wxqt-grib/docs/mag-reference"
inv = json.load(open('/tmp/claude-1000/mag/mag_inventory.json'))
def fetch(url, binary=False):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    try:
        d = urllib.request.urlopen(req, timeout=40).read()
        return d if binary else d.decode('utf8', 'replace')
    except Exception:
        return None
log = open(OUT + "/_log.txt", "a")
def say(s):
    print(s, flush=True); log.write(s + "\n"); log.flush()
only = sys.argv[1:]
for model, v in inv.items():
    if only and model not in only: continue
    area, prods = v['area'], v['products']
    if not area or not prods: say(f"{model}: nothing to grab"); continue
    d = f"{OUT}/{model}"; os.makedirs(d, exist_ok=True)
    have = {p['id'] for p in prods if os.path.exists(f"{d}/{p['id']}.gif")}
    if len(have) >= len(prods) - 3: say(f"{model}: already {len(have)}/{len(prods)}"); continue
    q = urllib.parse.quote(model); sec = area.lower(); m = model.lower()
    page = fetch(f"https://mag.ncep.noaa.gov/model-guidance-model-parameter.php?group=Model%20Guidance&model={q}&area={sec}&ps=area") or ""
    cycles = re.findall(r"data-cycle-date='([0-9]{8}) ([0-9]{2}) UTC", page)
    cycles = list(dict.fromkeys(cycles))[:4]
    first = prods[0]['id']
    pattern = None
    for date, hh in cycles:
        fh = fetch(f"https://mag.ncep.noaa.gov/model-fhrs.php?group=Model%20Guidance&model={m}&fhrmode=image&loopstart=-1&loopend=-1&area={sec}&fourpan=no&imageSize=&preselectedformattedcycledate={date}{hh}&cycle={date}{hh}&param={first}&ps=area") or ""
        hours = [int(x) for x in re.findall(r'fhr_valid">([0-9]+)', fh)]
        if not hours: continue
        pick = min(hours, key=lambda h: abs(h - 24))
        fmt = re.search(r'name="imagepath_format1" value="([^"]*)"><input type="hidden" name="imagepath_format2" value="([^"]*)"', fh)
        if not fmt: continue
        f1, f2 = fmt.group(1), fmt.group(2)
        width = max(len(str(h)) for h in re.findall(r'fhr_valid">([0-9]+)', fh))
        hs = f"{pick:0{width}d}"
        pattern = (hh, hs, f1, f2, date)
        break
    if not pattern: say(f"{model}: no valid cycle or hours found"); continue
    hh, hs, f1, f2, date = pattern
    got = len(have); fails = 0
    for p in prods:
        pid = p['id']; target = f"{d}/{pid}.gif"
        if os.path.exists(target): continue
        path = f1 + hh + f2.replace('%', hs).replace(first, pid)
        data = fetch("https://mag.ncep.noaa.gov/" + path, True); time.sleep(0.5)
        if data and data[:3] == b'GIF':
            open(target, 'wb').write(data); got += 1
        else:
            fails += 1
    say(f"{model}: {got}/{len(prods)} images (area {area}, cycle {date}{hh}, hour {hs}); first url {f1}{hh}{f2.replace('%', hs)}")
