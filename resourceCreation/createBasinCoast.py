#!/usr/bin/env python3
"""Builds resourceCreation/res/nhc_basins.bin: the coastlines and country borders of the Atlantic and Eastern / Central Pacific basins for the hurricane map.

Source: Natural Earth 1:50m coastline and land boundary lines (public domain, naturalearthdata.com), clipped to the basin and
simplified. File layout: little-endian float32 pairs (longitude, latitude); a pair of NaN ends each line.
Usage: createBasinCoast.py ne_50m_coastline.geojson ne_50m_admin_0_boundary_lines_land.geojson [tolerance-degrees]
"""
import json, math, struct, sys

WEST, EAST, SOUTH, NORTH = -180.0, 25.0, -8.0, 65.0

def simplify(points, tol):
    if len(points) < 3:
        return points
    keep = [False] * len(points)
    keep[0] = keep[-1] = True
    stack = [(0, len(points) - 1)]
    while stack:
        a, b = stack.pop()
        (ax, ay), (bx, by) = points[a], points[b]
        dx, dy = bx - ax, by - ay
        norm = math.hypot(dx, dy)
        far, index = -1.0, -1
        for i in range(a + 1, b):
            px, py = points[i]
            d = math.hypot(px - ax, py - ay) if norm == 0 else abs(dy * (px - ax) - dx * (py - ay)) / norm
            if d > far:
                far, index = d, i
        if far > tol:
            keep[index] = True
            stack += [(a, index), (index, b)]
    return [p for p, k in zip(points, keep) if k]

def lines(path):
    for feature in json.load(open(path))["features"]:
        geometry = feature["geometry"]
        parts = geometry["coordinates"] if geometry["type"] == "MultiLineString" else [geometry["coordinates"]]
        for part in parts:
            yield [(p[0], p[1]) for p in part]

def clipped(points):
    """the runs of points inside the box (with one point of margin so a line leaves the box rather than stopping short)"""
    run = []
    for p in points:
        inside = WEST - 2 <= p[0] <= EAST + 2 and SOUTH - 2 <= p[1] <= NORTH + 2
        if inside:
            run.append(p)
        elif run:
            yield run
            run = []
    if run:
        yield run

def main():
    tolerance = float(sys.argv[3]) if len(sys.argv) > 3 else 0.04
    out = bytearray()
    count = 0
    for path in sys.argv[1:3]:
        for line in lines(path):
            for run in clipped(line):
                run = simplify(run, tolerance)
                if len(run) < 2:
                    continue
                for lon, lat in run:
                    out += struct.pack("<ff", lon, lat)
                out += struct.pack("<ff", float("nan"), float("nan"))
                count += 1
    open("resourceCreation/res/nhc_basins.bin", "wb").write(out)
    print(count, "lines,", len(out), "bytes")

main()
