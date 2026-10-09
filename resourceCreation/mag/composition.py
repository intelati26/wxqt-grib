import json, glob, os, re, subprocess, sys
from PIL import Image, ImageOps
ROOT = "/home/mitch/Claude/wxqt-grib/docs/mag-reference"
inv = json.load(open('/tmp/claude-1000/mag/mag_inventory.json'))
cache_path = "/tmp/claude-1000/mag/captions.json"
cache = json.load(open(cache_path)) if os.path.exists(cache_path) else {}
def caption(path):
    im = Image.open(path).convert('L')
    w, h = im.size
    texts = []
    # the layer list is in the bottom caption; its position differs between models, so read the lowest 15 % and the top header
    for box in ((0, int(h*0.86), w, h), (0, 0, w, int(h*0.12))):
        c = im.crop(box); c = c.resize((c.width*2, c.height*2), Image.LANCZOS); c = ImageOps.autocontrast(c)
        c.save('/tmp/claude-1000/mag/_c.png')
        t = subprocess.run(['tesseract', '/tmp/claude-1000/mag/_c.png', '-', '--psm', '6'], capture_output=True, text=True).stdout
        texts.append(' | '.join(l.strip() for l in t.splitlines() if l.strip()))
    return texts
for model in inv:
    for p in inv[model]['products']:
        f = f"{ROOT}/{model}/{p['id']}.gif"
        key = f"{model}/{p['id']}"
        if key in cache or not os.path.exists(f): continue
        try: cache[key] = caption(f)
        except Exception as e: cache[key] = ["", str(e)]
    json.dump(cache, open(cache_path, 'w'))
print(len(cache), "captions")
