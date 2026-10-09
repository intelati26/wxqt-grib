import json, re, collections
inv = json.load(open('mag_inventory.json'))
cap = json.load(open('captions.json'))
drawn = collections.defaultdict(dict)
for l in open('/tmp/claude-1000/-home-mitch-Claude/5535068e-ba6b-4291-8c95-f9028c029e58/scratchpad/drawn.tsv'):
    p = l.rstrip('\n').split('\t')
    if len(p) == 3: drawn[p[0]][p[1]] = p[2]
native = {'GFS': 'GFS', 'NBM': 'NBM', 'AIGFS': 'AIGFS', 'GEFS-MEAN-SPRD': 'GEFS'}
def clean(foot):
    s = foot.replace('»', ',').replace('¥', 'V').replace('|', ' ')
    s = re.sub(r'\s+', ' ', s).strip()
    t = re.sub(r'^.*?[Vv]\s?[0-9O]{2,5}[A-Z0-9]?\s+', '', s, count=1)   # up to the forecast hour token: what is left is the layer list
    t = re.sub(r'^.*?\d{4}/\d{4}\S*\s+', '', t, count=1) if t == s else t
    return re.sub(r',\s*,', ',', t).strip(' ,')
rules = [('MSLP lines', r'EMSL|MSLP|PMSL|SEA LEVEL|SLP'), ('height lines', r'HGHT|HEIGHT| HT'), ('wind barbs/speed', r'WND|WIND|KTS|BARB'), ('isotachs (speed fill)', r'ISOTACH'),
         ('precipitation', r'PRECIP|PCPN|PRCP|PCP|QPF|RAIN|SNOW|SLEET'), ('temperature', r'TEMP|TMP|DEW|APPAR|CHILL|HEAT'), ('thickness lines', r'THICK'),
         ('isotherms', r'ISOTHERM'), ('humidity', r'REL HUM|RH |HUMID|PWAT|PRECIPITABLE'), ('vorticity', r'VORT'), ('CAPE / stability', r'CAPE|CIN|LIFTED|SHEAR|HELIC|SRH|UPHL|STP|SCP|LAPSE|LCL'),
         ('radar / satellite', r'REFL|RADAR|SIM|BRIGHT|CLOUD|ECHO'), ('probability', r'PROB|PERCENT|CHANCE|EXCEED'), ('ensemble stats', r'MEAN|SPREAD|SPRD|ENSEMBLE|MEMBERS?|MEDIAN|PCTL'),
         ('waves / ocean', r'WAVE|SWELL|PERIOD|SURGE|SST|DRIFT'), ('visibility / ceiling', r'VIS|CEIL|FOG'), ('fire / smoke', r'FIRE|HAINES|SMOKE|VENT|MIXING|HDW'), ('lightning', r'LIGHTN|FLASH')]
out = ['# MAG rendered maps: what each product draws', '',
       'Read from the footer caption of each model guidance image (OCR, so words can be a little off), with the layers it names tagged. "native" marks a product we already draw from GRIB with the same id. Images are saved locally in `docs/mag-reference/<MODEL>/<product>.gif` (not in git). Generated from the images downloaded on 2026-10-08.', '']
tagcount = collections.Counter(); total = 0; unavailable = []; missing = []
for model, v in inv.items():
    prods = v['products']
    if not prods: continue
    rows = []
    for p in prods:
        key = f"{model}/{p['id']}"
        if key not in cap:
            missing.append(key); continue
        foot = clean(cap[key][0]); up = foot.upper()
        tags = [t for t, rx in rules if re.search(rx, up)]
        for t in tags: tagcount[t] += 1
        total += 1
        n = native.get(model)
        rows.append((p['id'], p['label'], foot, ', '.join(tags), 'native' if n and p['id'] in drawn[n] else ''))
    if not rows: unavailable.append(model); continue
    out += [f"## {model}  ({len(rows)} of {len(prods)} maps read; {sum(1 for r in rows if r[4])} drawn natively)", '', '| product | label | the map draws | layers | |', '|---|---|---|---|---|']
    out += [f"| `{r[0]}` | {r[1]} | {r[2].replace('|','/')} | {r[3]} | {r[4]} |" for r in rows]
    out.append('')
out += ['## Not fetched', '', ', '.join(unavailable) + ' - the image paths the model pages advertise return nothing for these models right now (no current images on the site, or different naming); to be matched from their data instead. Products of other models that did not download: ' + ', '.join(missing) + '.', '', f'## Layer counts over the {total} maps read', '']
out += [f"- {t}: {n}" for t, n in tagcount.most_common()]
open('/home/mitch/Claude/wxqt-grib/docs/mag-composition.md', 'w').write('\n'.join(out) + '\n')
print(total, 'maps;', len(unavailable), 'models without images')
