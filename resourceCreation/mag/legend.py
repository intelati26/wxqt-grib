import sys, subprocess, re, json
from PIL import Image
def swatches(path):
    im = Image.open(path).convert('RGB'); w, h = im.size
    px = im.load()
    best = None
    for x in range(2, 80):
        runs = []; start = 0; cur = px[x, 0]
        for y in range(1, h):
            c = px[x, y]
            if c != cur:
                if y - start >= 7 and cur not in ((255, 255, 255), (0, 0, 0)): runs.append((start, y - 1, cur))
                start = y; cur = c
        # the legend column has many consecutive runs of distinct colors, touching each other
        chain = 0; maxchain = 0; last = None
        for r in runs:
            if last is not None and r[0] - last[1] <= 3: chain += 1
            else: chain = 1
            maxchain = max(maxchain, chain); last = r
        score = maxchain
        if best is None or score > best[0]: best = (score, x, runs)
    return best, im
def labels(im, x0, y0, y1):
    crop = im.crop((x0 + 6, max(y0 - 12, 0), x0 + 60, min(y1 + 12, im.height))).convert('L')
    crop = crop.resize((crop.width * 4, crop.height * 4), Image.LANCZOS)
    crop.save('/tmp/claude-1000/mag/_lg.png')
    out = subprocess.run(['tesseract', '/tmp/claude-1000/mag/_lg.png', '-', '--psm', '11', '-c', 'tessedit_char_whitelist=0123456789.-', 'tsv'], capture_output=True, text=True).stdout.splitlines()
    res = []
    for l in out[1:]:
        p = l.split('\t')
        if len(p) >= 12 and p[11].strip() and re.fullmatch(r'-?[0-9]+\.?[0-9]*', p[11].strip()):
            res.append((max(y0 - 12, 0) + (int(p[7]) + int(p[9]) / 2) / 4, p[11].strip()))
    return res
if __name__ == '__main__':
    for path in sys.argv[1:]:
        (score, x, runs), im = swatches(path)
        print(path.split('mag-reference/')[1], 'legend x', x, 'chain', score, 'swatches', len(runs))
        if runs:
            lab = labels(im, x, runs[0][0], runs[-1][1])
            print('   colors top->bottom:', [('%d-%d' % (r[0], r[1]), '#%02x%02x%02x' % r[2]) for r in runs][:30])
            print('   labels (y, text):', [(round(y), t) for y, t in lab])
