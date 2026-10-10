import subprocess, sys, os
from PIL import Image, ImageDraw
R = "/home/mitch/Claude/wxqt-grib"
OUT = R + "/docs/compare"
# (MAG model, product, our model, run, hour)
pairs = [
 ("GFS", "precip_p24", "precip_p24", "GFS", "2026100818", 24), ("GFS", "500_rh_ht", "500_rh_ht", "GFS", "2026100818", 24), ("GFS", "precip_rate_type", "precip_type", "GFS", "2026100818", 24),
 ("GFS", "200_wnd_ht", "200_wnd_ht", "GFS", "2026100818", 24), ("NAM-HIRES", "sim_radar_1km", "sim_radar_1km", "RRFS", "2026100818", 24), ("GFS", "10m_wnd_2m_temp", "10m_wnd_2m_temp", "GFS", "2026100818", 24),
]
only = sys.argv[1:]
for mag, prod, prodOurs, ours, run, hour in pairs:
    if only and f"{mag}/{prod}" not in only: continue
    ref = f"{R}/docs/mag-reference/{mag}/{prod}.gif"
    mine = f"/tmp/claude-1000/mag/cmp_{mag}_{prod}.png"
    env = dict(os.environ, DEMO_RUN=run)
    r = subprocess.run([R + "/tests/gfs/demo.sh", prodOurs, "CONUS", str(hour), mine, ours], capture_output=True, text=True, env=env, timeout=900)
    if not os.path.exists(mine) or "wrote" not in r.stdout:
        print(f"{mag}/{prod}: FAILED {r.stdout.strip().splitlines()[-1:]}", flush=True); continue
    a = Image.open(ref).convert("RGB"); b = Image.open(mine).convert("RGB")
    h = 900
    a = a.resize((int(a.width * h / a.height), h), Image.LANCZOS); b = b.resize((int(b.width * h / b.height), h), Image.LANCZOS)
    sheet = Image.new("RGB", (a.width + b.width + 30, h + 40), "white")
    d = ImageDraw.Draw(sheet)
    d.text((10, 8), f"MAG  {mag}/{prod}  ({run} f{hour:03d})", fill="black")
    d.text((a.width + 30, 8), f"ours  {ours}/{prod}", fill="black")
    sheet.paste(a, (0, 40)); sheet.paste(b, (a.width + 30, 40))
    path = f"{OUT}/{mag}_{prod}.png"; sheet.save(path)
    print(f"{mag}/{prod}: ok", flush=True)
