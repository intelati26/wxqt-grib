#!/usr/bin/env python3
"""Reads every SPC REFS chart id as product x window x type (+ limit): spcrefs-grid.csv (one row per id) and spcrefs-matrix.csv (product by type, with the windows each has)."""
import re, csv, collections, os
here = os.path.dirname(os.path.abspath(__file__))
rows = []
for l in open(os.path.join(here, 'charts-dump.txt')):
    if l.startswith('CHART SPCREFS|'):
        f = l[6:].rstrip('\n').split('|'); rows.append(dict(cat=f[1], id=f[2], lab=f[3]))
TYPES = [('neighborhood probability', 'Neighborhood probability'), ('paintball', 'Paintball'), ('ensemble probability', 'Ensemble probability'), ('ensemble maximum', 'Ensemble max'), ('ensemble minimum', 'Ensemble min'),
         ('ensemble mean', 'Ensemble mean'), ('pm mean', 'PM mean'), ('one member', 'One member')]
def parse(r):
    lab = r['lab']; ll = lab.lower()
    typ = next((n for p, n in TYPES if p in ll), '')
    if not typ and ('probability' in ll): typ = 'Ensemble probability'
    m = re.match(r'(\d+)-h ', lab); win = (m.group(1) + ' h') if m else ''
    prod = lab
    prod = re.sub(r'^\d+-h ', '', prod)
    prod = re.sub(r'\([^)]*\)', '', prod)
    prod = re.sub(r', \d+-km neighborhood probability', '', prod)
    prod = re.sub(r' Paintball', '', prod)
    prod = re.sub(r'(>|<)\s*-?[\d.]+(?:p)?\s*(?:dBZ|m/s|kt|J/kg|in|m2/s2|percentile|mph|%)?', '', prod)
    prod = re.sub(r'\bshown as magnitude\b|, smoothed', '', prod)
    prod = re.sub(r'^(Max|Min) ', '', prod)
    prod = re.sub(r'^Paintball ', '', prod)
    prod = re.sub(r' where .*$', '', prod)
    prod = re.sub(r'\bUH\b', 'Updraft Helicity', prod)
    prod = prod.replace('SBCAPE', 'Surface-Based CAPE').replace('Fixed-Layer STP', 'Fixed-Layer Significant Tornado Parameter')
    prod = re.sub(r'^(Max |Min )', '', prod)
    prod = re.sub(r'\s+', ' ', prod).strip(' ,')
    prod = re.sub(r' ,.*$', '', prod).strip()
    lims = re.findall(r'(>|<|between)\s*(-?[\d.]+)\s*(dBZ|m/s|kt|J/kg|in|m2/s2|percentile|mph|%|mi)?', lab)
    lims = [' '.join(x for x in l if x) for l in lims]
    window = win or ('instant' if not re.search(r'\d+-h|1-h', lab) else '')
    fit = 'fits'
    if not typ: fit = 'no type named (' + ('a combination' if 'and' in ll else 'check') + ')'
    elif len(lims) > 1 or 'between' in ll or 'blizzard' in ll or ' and ' in ll and 'where' not in ll and 'qpf and' in ll: fit = 'two or more limits / combination'
    if 'dominant precipitation type' in ll: fit = 'a combination (QPF with a type)'
    if 'blizzard' in ll: fit = 'two or more limits / combination'
    return dict(id=r['id'], label=lab, category=r['cat'], product=prod, window=window, type=typ, limit=' & '.join(lims), fit=fit)
ps = [parse(r) for r in rows]
with open(os.path.join(here, 'spcrefs-grid.csv'), 'w', newline='') as f:
    w = csv.DictWriter(f, ['id', 'label', 'category', 'product', 'window', 'type', 'limit', 'fit']); w.writeheader(); w.writerows(ps)
prods = collections.defaultdict(lambda: collections.defaultdict(set))
for p in ps:
    if p['fit'] == 'fits': prods[p['product']][p['type']].add(p['window'])
alltypes = [n for _, n in TYPES]
with open(os.path.join(here, 'spcrefs-matrix.csv'), 'w', newline='') as f:
    w = csv.writer(f); w.writerow(['product'] + alltypes + ['charts']); 
    for pr in sorted(prods, key=lambda x: -sum(len(v) for v in prods[x].values())):
        w.writerow([pr] + ['/'.join(sorted(prods[pr][t], key=lambda x: (x == 'instant', int(re.sub(r'\D', '', x) or 0)))) if prods[pr][t] else '' for t in alltypes] + [sum(1 for p in ps if p['fit'] == 'fits' and p['product'] == pr)])
c = collections.Counter(p['fit'] for p in ps)
print(f'SPCREFS: {len(ps)} ids; {dict(c)}; products {len(prods)}')
