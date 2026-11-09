#!/usr/bin/env python3
# *****************************************************************************
# * This file is part of wxqt.  Licensed under the GNU General Public License v3.
# * See the COPYING file for the full license text.
# *****************************************************************************
#
# Builds the compact normals file from NOAA's 1/24 degree (5 km) U.S. daily gridded climate normals (1991-2020, NCEI Accession 0259962), which
# Microsoft's Planetary Computer serves as one cloud-optimized GeoTIFF per day of a 366 day year (about 0.87 MB each, 319 MB for a year).
#
# The rule (see docs/normals-storage.md): never keep or fetch the whole year. The daily normal is a smooth seasonal curve, so
# a mean and six sine/cosine pairs per cell, fitted through 15 sample days (every 26th), reproduce every one of the 366 days to 0.08 F rms
# (0.65 F worst). The file keeps the 13 coefficients per land cell as 16 bit steps of 0.05 F (differenced along the cell order, then zlib):
# about 2.6 MB per variable, from a 13 MB one-time download that is deleted after the fit.
#
# Usage:  createNormals.py --var tmax_norm --out tmax_norm.nrm [--step 26] [--harmonics 6] [--quant 0.05] [--work DIR] [--verify]
# Needs numpy and gdal_translate on the PATH. Fetches politely (one request a second, a plain "wxqt" user agent).

import argparse, json, os, struct, subprocess, sys, time, urllib.request, zlib
import numpy as np

BASE = "https://noaanormals.blob.core.windows.net/gridded-normals-cogs/normals-daily/1991-2020"
TOKEN = "https://planetarycomputer.microsoft.com/api/sas/v1/token/noaanormals/gridded-normals-cogs"
WIDTH, HEIGHT, LON0, LAT0, STEP = 1385, 596, -124.70833333, 49.37500127, 0.04166667   # the grid of every file (cell edges, north-west corner)
YEAR = 366   # their calendar has Feb 29: day 59 is Feb 28, day 60 Feb 29, day 314 is Nov 9

def token():
    req = urllib.request.Request(TOKEN, headers={"User-Agent": "wxqt"})
    return json.load(urllib.request.urlopen(req, timeout=30))["token"]

def fetch(day, var, work, tok):
    tif = os.path.join(work, "d%03d.tif" % day)
    raw = os.path.join(work, "d%03d.raw" % day)
    if not os.path.exists(raw):
        if not os.path.exists(tif):
            url = "%s/1991_2020-daily-%03d-%s.tif?%s" % (BASE, day, var, tok)
            with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "wxqt"}), timeout=90) as r, open(tif, "wb") as f:
                f.write(r.read())
            time.sleep(1)
        subprocess.run(["gdal_translate", "-q", "-of", "ENVI", "-ot", "Float32", tif, raw], check=True, stdout=subprocess.DEVNULL)
    return np.fromfile(raw, dtype="<f4")

def basis(days, k):
    t = 2 * np.pi * (np.asarray(days, float) - 1) / YEAR
    cols = [np.ones(len(days))]
    for h in range(1, k + 1):
        cols += [np.cos(h * t), np.sin(h * t)]
    return np.column_stack(cols)

def build(args):
    os.makedirs(args.work, exist_ok=True)
    days = list(range(1, YEAR + 1, args.step))
    tok = token()
    grids = [fetch(d, args.var, args.work, tok) for d in days]
    ok = np.isfinite(np.array(grids)).all(0)
    unit = 1.8 if args.var.startswith("t") else 1.0   # the files are degrees C; the file is degrees F (differences scale by 1.8, the 32 offset goes into the mean)
    shift = 32.0 if args.var.startswith("t") else 0.0
    y = np.array(grids)[:, ok] * unit + shift
    c = np.linalg.lstsq(basis(days, args.harmonics), y, rcond=None)[0]
    q = np.round(c / args.quant).astype(np.int32)
    planes = b"".join(np.diff(q[r], prepend=0).astype(np.int16).tobytes() for r in range(q.shape[0]))
    mask = np.packbits(ok.astype(np.uint8))
    head = struct.pack("<4sHHHHffffHfI", b"NRM1", WIDTH, HEIGHT, args.harmonics, YEAR, LON0, LAT0, STEP, unit, 0, args.quant, int(ok.sum()))
    body = zlib.compress(mask.tobytes(), 9)
    data = zlib.compress(planes, 9)
    with open(args.out, "wb") as f:
        f.write(head + struct.pack("<II", len(body), len(data)) + body + data)
    print("wrote %s: %.2f MB (%d land cells, %d sample days, %d harmonics)" % (args.out, os.path.getsize(args.out) / 1e6, ok.sum(), len(days), args.harmonics))

def read(path):
    """Returns (valid mask [HEIGHT*WIDTH], coefficients [2K+1, cells] in the file's unit, K)."""
    b = open(path, "rb").read()
    n = struct.calcsize("<4sHHHHffffHfI")
    magic, w, h, k, year, lon0, lat0, step, unit, _, quant, cells = struct.unpack("<4sHHHHffffHfI", b[:n])
    assert magic == b"NRM1"
    lb, ld = struct.unpack("<II", b[n:n + 8])
    mask = np.unpackbits(np.frombuffer(zlib.decompress(b[n + 8:n + 8 + lb]), np.uint8))[:w * h].astype(bool)
    d = np.frombuffer(zlib.decompress(b[n + 8 + lb:n + 8 + lb + ld]), np.int16).reshape(2 * k + 1, cells).astype(np.int32)
    return mask, np.cumsum(d, axis=1) * quant, k

def normal(coefficients, k, day):
    return basis([day], k)[0] @ coefficients

def verify(args):
    mask, c, k = read(args.out)
    have = [d for d in range(1, YEAR + 1) if os.path.exists(os.path.join(args.work, "d%03d.raw" % d))]
    unit = 1.8 if args.var.startswith("t") else 1.0
    shift = 32.0 if args.var.startswith("t") else 0.0
    worst, total, count = 0.0, 0.0, 0
    for d in have:
        g = np.fromfile(os.path.join(args.work, "d%03d.raw" % d), dtype="<f4")[mask] * unit + shift
        e = normal(c, k, d) - g
        worst = max(worst, np.abs(e).max()); total += (e ** 2).sum(); count += e.size
    print("verified on %d days: rms %.3f, worst %.2f" % (len(have), (total / count) ** 0.5, worst))

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--var", default="tmax_norm")
    p.add_argument("--out", required=True)
    p.add_argument("--step", type=int, default=26)
    p.add_argument("--harmonics", type=int, default=6)
    p.add_argument("--quant", type=float, default=0.05)
    p.add_argument("--work", default="normals_work")
    p.add_argument("--verify", action="store_true", help="also check against every daily file found in --work")
    a = p.parse_args()
    build(a)
    if a.verify:
        verify(a)
