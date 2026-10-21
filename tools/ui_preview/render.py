import json,sys,colorsys
from PIL import Image,ImageDraw,ImageFont
SC=3
def col(c):
    a=(c>>24)&255; b=(c>>16)&255; g=(c>>8)&255; r=c&255
    return (r,g,b,a)
fonts={}
def font(px,weight='Medium'):
    k=(round(px),weight)
    if k not in fonts: fonts[k]=ImageFont.truetype('/usr/share/fonts/opentype/inter/Inter-%s.otf'%weight,max(4,round(px)))
    return fonts[k]
def render(src,dst):
    import re
    lines=[json.loads(re.sub(r'-?nan','0',l)) for l in open(src)]
    meta=lines[0]; W,H=meta['w'],meta['h']
    img=Image.new('RGBA',(W*SC,H*SC),(0,0,0,255))
    d=ImageDraw.Draw(img,'RGBA')
    def blend(box_fn,c):
        # draw with alpha using a layer
        layer=Image.new('RGBA',img.size,(0,0,0,0)); ld=ImageDraw.Draw(layer,'RGBA'); box_fn(ld,c)
        img.alpha_composite(layer)
    for o in lines[1:]:
        t=o['t']
        if t in('rect','rrect') and (o['w']<=0 or o['h']<=0): continue
        if t=='rect':
            c=col(o['c'])
            if c[3]==0: continue
            blend(lambda ld,c:ld.rectangle([o['x']*SC,o['y']*SC,(o['x']+o['w'])*SC-1,(o['y']+o['h'])*SC-1],fill=c),c)
        elif t=='rrect':
            c=col(o['c'])
            if c[3]==0: continue
            r=min(o['r'],o['h']/2,o['w']/2)
            blend(lambda ld,c:ld.rounded_rectangle([o['x']*SC,o['y']*SC,(o['x']+o['w'])*SC-1,(o['y']+o['h'])*SC-1],radius=r*SC,fill=c),c)
        elif t=='circle':
            c=col(o['c']);r=o['r']
            blend(lambda ld,c:ld.ellipse([(o['x']-r)*SC,(o['y']-r)*SC,(o['x']+r)*SC,(o['y']+r)*SC],fill=c),c)
        elif t=='line':
            c=col(o['c'])
            blend(lambda ld,c:ld.line([o['x']*SC,o['y']*SC,o['x1']*SC,o['y1']*SC],fill=c,width=max(1,round(o['w']*SC))),c)
        elif t=='text':
            c=col(o['c']); em=26*o['s']
            f=font(em*SC)
            for i,ln in enumerate(o['v'].split('\n')):
                blend(lambda ld,c:ld.text((o['x']*SC,(o['y']+0.77*em+i*20*o['s'])*SC),ln,font=f,fill=c,anchor='ls'),c)
        elif t=='thumb':
            h=(o['k']*0.17)%1; r,g,b=[int(x*255) for x in colorsys.hsv_to_rgb(h,0.45,0.55)]
            x,y,w,hh=o['x'],o['y'],o['w'],o['h']
            for i in range(int(hh*SC)):
                f=0.75+0.25*i/(hh*SC)
                d.line([x*SC,y*SC+i,(x+w)*SC-1,y*SC+i],fill=(int(r*f),int(g*f),int(b*f),255))
        elif t=='tex':
            d.rectangle([o['x']*SC,o['y']*SC,(o['x']+o['w'])*SC-1,(o['y']+o['h'])*SC-1],outline=(200,200,200,255))
    img.convert('RGB').save(dst)
if __name__=='__main__': render(sys.argv[1],sys.argv[2])
