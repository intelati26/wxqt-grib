import re, json, time, urllib.request, urllib.parse
UA="wxqt-inventory (one-off list of the model guidance parameter names)"
def get(url):
    req=urllib.request.Request(url,headers={"User-Agent":UA})
    return urllib.request.urlopen(req,timeout=40).read().decode('utf8','replace')
h=open('/tmp/mag_models.html').read()
models=[(m[1],m[3]) for m in re.findall(r'<a id="modtype_([^"]+)" class="model_link[^"]*" href="#" data-model="([^"]+)" data-group="([^"]+)" title="([^"]*)">',h)]
areas=re.findall(r'<a id="modarea_([^"]+)" class="area_link',h)
print(len(models),"models",len(areas),"areas")
pref=["CONUS","NAMER","GLOBAL","ALASKA","US-EAST","WEST-ATL","ATLANTIC","NORTH-PAC","ARCTIC","ARTIC"]
order=pref+[a for a in areas if a not in pref]
inv={}
for model,desc in models:
    found=None
    for area in order[:9]:
        url=f"https://mag.ncep.noaa.gov/model-guidance-model-parameter.php?group=Model%20Guidance&model={urllib.parse.quote(model)}&area={area}&ps=area"
        try:
            page=get(url)
        except Exception as e:
            time.sleep(0.7); continue
        items=re.findall(r"<a class='params_link[^']*' id='([^']+)'[^>]*title='([^']*)'",page)
        time.sleep(0.7)
        if items:
            found=(area,items); break
    inv[model]={"description":desc,"area":found[0] if found else None,"products":[{"id":i,"label":t} for i,t in (found[1] if found else [])]}
    print(f"{model:16s} area={inv[model]['area']} products={len(inv[model]['products'])}")
json.dump(inv,open('mag_inventory.json','w'),indent=1)
