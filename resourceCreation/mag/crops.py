import sys
from PIL import Image, ImageDraw
from legend import swatches
R='/home/mitch/Claude/wxqt-grib/docs/mag-reference/'
def crop(name, out, scale=3):
    (score,x,runs),im=swatches(R+name)
    # the legend is the longest chain of touching swatches
    chain=[]; best=[]
    for r in runs:
        if chain and r[0]-chain[-1][1]<=3: chain.append(r)
        else:
            chain=[r]
        if len(chain)>len(best): best=list(chain)
    y0,y1=best[0][0]-18,best[-1][1]+18
    box=(max(x-12,0),max(y0,0),min(x+75,im.width),min(y1,im.height))
    c=im.crop(box).resize(((box[2]-box[0])*scale,(box[3]-box[1])*scale),Image.NEAREST)
    d=ImageDraw.Draw(c)
    for i,r in enumerate(best):
        yy=((r[0]+r[1])//2-box[1])*scale
        d.text((2,yy-5),str(i),fill=(255,0,0))
    c.save(out)
    return best
if __name__=='__main__':
    names=sys.argv[2:]
    sheets=[]
    for n in names:
        best=crop(n,'/tmp/claude-1000/mag/_c_%s.png'%n.replace('/','_'))
        sheets.append((n,best,Image.open('/tmp/claude-1000/mag/_c_%s.png'%n.replace('/','_'))))
    W=sum(s[2].width+20 for s in sheets); H=max(s[2].height for s in sheets)+25
    sheet=Image.new('RGB',(W,H),'white'); d=ImageDraw.Draw(sheet); x=0
    for n,best,im in sheets:
        d.text((x+2,4),n.split('/')[-1][:24]+' (%d)'%len(best),fill='black'); sheet.paste(im,(x,22)); x+=im.width+20
    sheet.save(sys.argv[1]); print(sheet.size,[ (n,len(b)) for n,b,_ in sheets])
