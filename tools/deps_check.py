#!/usr/bin/env python3
"""The packages' dependencies against their newest releases: what the daily check (.github/workflows/catalog-check.yml) says about "stale" and "behind".

For each dependency: the version the packages are built with (vcpkg's port on the runner, the Qt and the Actions the workflows name) and the newest release of its own project.
A difference in the MAJOR or MINOR number (3.12 against 3.13) is "behind" and is what the issue is opened for; a difference in the patch number only (3.13.2 against 3.13.3) is listed as
"patch".  Sources of the newest release: GitHub releases (or tags), GitLab's atom feed of the tags (libtiff), the SQLite project's GitHub mirror (its tags' atom feed).

  deps_check.py [--ports DIR | --ports-from-github] [--workflows DIR] [--out FILE.md] [--issue FILE.md]

Writes the table to --out, the issue text to --issue, prints "behind=N patch=N" for the workflow's output.  GDAL is left to the step of its own (the overlay port's version against the newest release).
"""
import argparse, base64, json, os, re, subprocess, sys, urllib.request

# (name, vcpkg port or None, upstream kind, upstream id, note)
# kinds: gh (GitHub releases, then tags: owner/repo), atom (a feed whose entry titles hold the version: URL), qt (qt/qtbase tags)
DEPENDENCIES = [
    ("PROJ", "proj", "gh", "OSGeo/PROJ", ""),
    ("GEOS", "geos", "gh", "libgeos/geos", ""),
    ("netcdf-c", "netcdf-c", "gh", "Unidata/netcdf-c", "vcpkg's patches for it do not apply to a newer release (checked 2026-11-18 against 4.10.1): wait for vcpkg's port, or port the patches"),
    ("HDF5", "hdf5", "gh", "HDFGroup/hdf5", ""),
    ("curl", "curl", "gh", "curl/curl", ""),
    ("libtiff", "tiff", "atom", "https://gitlab.com/libtiff/libtiff/-/tags?format=atom", "no GitHub releases: GitLab's tag feed"),
    ("SQLite", "sqlite3", "atom", "https://github.com/sqlite/sqlite/tags.atom", "no releases feed of its own: the project's GitHub mirror (its tags' feed; https://sqlite.org/src/timeline.rss is the project's own)"),
    ("libpng", "libpng", "gh-stable", "pnggroup/libpng", "the newest tag is a 1.7 beta: only a release counts"),
    ("OpenJPEG", "openjpeg", "gh", "uclouvain/openjpeg", ""),
    ("libaec", "libaec", "gh", "MathisRosenhauer/libaec", ""),
    ("zlib", "zlib", "gh", "madler/zlib", ""),
    ("expat", "expat", "gh", "libexpat/libexpat", ""),
    ("json-c", "json-c", "gh", "json-c/json-c", ""),
    ("libgeotiff", "libgeotiff", "gh", "OSGeo/libgeotiff", ""),
    ("libwebp", "libwebp", "gh", "webmproject/libwebp", "the tag names are not versions: skipped when it cannot be read"),
    ("OpenSSL", "openssl", "gh", "openssl/openssl", "vcpkg follows the 3.x series: a new major version (4.x) is not taken as it comes"),
    ("Qt (Windows: vcpkg qtbase)", "qtbase", "qt", "qt/qtbase", "vcpkg's port: a patched series that lags the newest minor by a little"),
    ("Qt (Linux: workflow pin)", None, "qt", "qt/qtbase", "the Linux version the workflows install (aqtinstall); Windows takes the version of vcpkg's qtbase port (below)"),
    ("actions/checkout", None, "action", "actions/checkout", ""),
    ("actions/cache", None, "action", "actions/cache", ""),
    ("actions/upload-artifact", None, "action", "actions/upload-artifact", ""),
    ("jurplel/install-qt-action", None, "action", "jurplel/install-qt-action", ""),
    ("mozilla-actions/sccache-action", None, "action", "mozilla-actions/sccache-action", ""),
]

def gh(path):
    r = subprocess.run(["gh", "api", path], capture_output=True, text=True)
    try:
        return json.loads(r.stdout)
    except Exception:
        return None

def fetch(url):
    try:
        with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "wxqt-deps-check"}), timeout=30) as r:
            return r.read().decode("utf-8", "replace")
    except Exception:
        return ""

def numbers(text):
    """the first dotted number in a tag: 'v3.13.3' 'curl-8_23_0' 'version-3.54.1' 'R_2_10_0' 'json-c-0.19-20260627' -> (3, 13, 3)"""
    text = re.sub(r"(?<=\d)_(?=\d)", ".", text)
    m = re.search(r"(\d+)\.(\d+)(?:\.(\d+))?", text)
    return tuple(int(g) for g in m.groups() if g is not None) + (0,) * (3 - len([g for g in m.groups() if g is not None])) if m else None

def stable(tag):
    return not re.search(r"(rc|beta|alpha|dev|pre|snapshot)", tag, re.I)

def newest(kind, ident):
    if kind in ("gh", "gh-stable", "action"):
        j = gh(f"repos/{ident}/releases/latest")
        if isinstance(j, dict) and j.get("tag_name") and stable(j["tag_name"]):
            return j["tag_name"]
        tags = (gh(f"repos/{ident}/tags?per_page=100") or []) + (gh(f"repos/{ident}/tags?per_page=100&page=2") or [])
        best = None
        for t in tags:
            n = t["name"]
            if stable(n) and numbers(n) and (best is None or numbers(n) > numbers(best)):
                best = n
        return best
    if kind == "atom":
        titles = re.findall(r"<title>([^<]*)</title>", fetch(ident))
        best = None
        for t in titles[1:]:
            m = re.search(r"(?:version-|Version |v)(\d+\.\d+(?:\.\d+)*)", t)
            if m and stable(t) and (best is None or numbers(m.group(1)) > numbers(best)):
                best = m.group(1)
        return best
    if kind == "qt":
        tags = [t["name"] for t in (gh(f"repos/{ident}/tags?per_page=60") or [])]
        rel = [t for t in tags if re.fullmatch(r"v6\.\d+\.\d+", t)]
        return max(rel, key=numbers) if rel else None
    return None

def port_version(name, ports_dir):
    if ports_dir:
        try:
            d = json.load(open(os.path.join(ports_dir, name, "vcpkg.json")))
        except Exception:
            return None
    else:
        j = gh(f"repos/microsoft/vcpkg/contents/ports/{name}/vcpkg.json")
        try:
            d = json.loads(base64.b64decode(j["content"]))
        except Exception:
            return None
    return d.get("version") or d.get("version-semver") or d.get("version-string") or d.get("version-date")

def workflow_pins(dirpath):
    pins = {}
    for f in sorted(os.listdir(dirpath)):
        if not f.endswith((".yml", ".yaml")):
            continue
        text = open(os.path.join(dirpath, f)).read()
        for m in re.finditer(r"uses:\s*([\w.-]+/[\w.-]+)(?:/[\w./-]+)?@(v[\d.]+)", text):
            pins[m.group(1)] = m.group(2)
        for m in re.finditer(r"install-qt-action@[^\n]*\n\s*with:(?:\s*#[^\n]*\n)*\s*version:\s*'([\d.]+)'", text):
            pins["Qt (Linux: workflow pin)"] = m.group(1)
    return pins

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ports", default=os.path.join(os.environ.get("VCPKG_INSTALLATION_ROOT", ""), "ports") if os.environ.get("VCPKG_INSTALLATION_ROOT") else "")
    ap.add_argument("--ports-from-github", action="store_true")
    ap.add_argument("--workflows", default=".github/workflows")
    ap.add_argument("--out", default="deps.md")
    ap.add_argument("--issue", default="deps-issue.md")
    a = ap.parse_args()
    ports = "" if a.ports_from_github else a.ports
    pins = workflow_pins(a.workflows)
    rows, behind, patch = [], [], []
    for name, port, kind, ident, note in DEPENDENCIES:
        have = port_version(port, ports) if port else pins.get(name)
        latest = newest(kind, ident)
        state = "?"
        if kind == "action" and have and latest and numbers(latest + ".0"):   # (an action's pin is its major: v5, or the whole v0.0.11)
            l = numbers(latest + ".0")
            if re.fullmatch(r"v\d+", have):
                state = "behind" if l[0] > int(have[1:]) else "current"
            else:
                state = "behind" if l[:2] > numbers(have + ".0")[:2] else ("patch" if l > numbers(have + ".0") else "current")
        elif have and latest and numbers(have) and numbers(latest):
            h, l = numbers(have), numbers(latest)
            if name == "OpenSSL" and l[0] > h[0]:
                state = "held"   # (vcpkg follows the 3.x series)
            elif h[:2] < l[:2]:
                state = "behind"
            elif h < l:
                state = "patch"
            else:
                state = "current"
        rows.append((name, have or "?", latest or "?", state, note))
        if state == "behind":
            behind.append(rows[-1])
        elif state == "patch":
            patch.append(rows[-1])
    lines = ["## Dependencies", "", f"{len(behind)} behind by a major or minor version, {len(patch)} by a patch only.", "", "| dependency | built with | newest | state | note |", "|---|---|---|---|---|"]
    for r in rows:
        lines.append(f"| {r[0]} | {r[1]} | {r[2]} | {r[3]} | {r[4]} |")
    open(a.out, "w").write("\n".join(lines) + "\n")
    body = ["These dependencies are behind their newest release by a major or minor version (vcpkg's port, the Qt of the workflows or an action's major):", ""]
    for r in behind:
        body.append(f"- **{r[0]}**: built with {r[1]}, newest {r[2]}" + (f" ({r[4]})" if r[4] else ""))
    body += ["", "Patch-only differences are in the run's summary. A vcpkg library moves when vcpkg's port does (or by an overlay port in .github/vcpkg/ports, as GDAL's); Qt and the actions are named in .github/workflows."]
    open(a.issue, "w").write("\n".join(body) + "\n")
    print(f"behind={len(behind)} patch={len(patch)}")

if __name__ == "__main__":
    main()
