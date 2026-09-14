"""Deterministic, palette-exact authored pixel exports. No noise/dithering.
Run from any directory with Python + Pillow. Does not modify characters-v1 sources.
Strategic city sprites use 32 texels/tile; actor/crop sprites use 16 texels/tile.
"""
from pathlib import Path
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'assets/sprites/ui-military-v1'
OUT.mkdir(parents=True,exist_ok=True)
PALETTE={line.strip().upper() for line in (ROOT/'config/art-palette.hex').read_text().splitlines() if line.strip().startswith('#') and len(line.strip())==7}
C={'ink':'#080F1B','purple':'#392B3C','earth':'#49352F','soil':'#74513F','road':'#A78D72',
   'plaster':'#D9C79F','ivory':'#EFE2CF','wall':'#9AA7AF','stone':'#596679','steel':'#394658',
   'roof':'#7A5038','tile':'#B78350','light':'#D5A454','gold':'#EBC46B','grain':'#F4D78B',
   'stem':'#A6CD59','field':'#235747','leaf':'#49975B','blue':'#46627D','navy':'#202C43'}
def col(name): return C[name]
def save(im,name):
    # Source texture color membership is strict; transparency has no RGB meaning.
    for p in im.getdata():
        if p[3] and ('#%02X%02X%02X'%p[:3]) not in PALETTE:
            raise ValueError((name,p))
    im.save(OUT/name)
# Reuse approved silhouettes/equipment. Four explicit poses; animation is driven
# by distance travelled, never wall time or idle jitter. Two swing poses preserve
# faces and torso while moving the feet and forearms by whole authored texels.
for source in sorted((ROOT/'assets/sprites/characters-v1').glob('*.png')):
    base=Image.open(source).convert('RGBA')
    if base.size!=(16,20): continue
    strip=Image.new('RGBA',(64,20))
    for frame in range(4):
        pose=Image.new('RGBA',base.size)
        for y in range(20):
            for x in range(16):
                rgba=base.getpixel((x,y))
                if not rgba[3]:continue
                dx=dy=0
                if frame in (1,3):
                    phase=1 if frame==1 else -1
                    if y>=15:
                        dy=phase if x<8 else -phase
                    elif 11<=y<=13 and (x<=4 or x>=11):
                        dy=-phase if x<8 else phase
                elif frame==2 and y<15:
                    dy=-1
                if 0<=x+dx<16 and 0<=y+dy<20:
                    pose.putpixel((x+dx,y+dy),rgba)
        strip.alpha_composite(pose,(frame*16,0))
    save(strip,source.name)
# Thin, separated stalks rather than one opaque bush. Dimensions are 12x8.
crop=Image.new('RGBA',(12,8)); d=ImageDraw.Draw(crop)
for x,y in [(1,7),(5,6),(9,7)]:
    d.line((x,y,x,y-5),fill=col('stem'))
    d.point((x-1,y-4),fill=col('gold'));d.point((x+1,y-3),fill=col('grain'))
    d.point((x,y-5),fill=col('grain'));d.point((x,y-2),fill=col('light'))
save(crop,'grain-stalks.png')
# Hand-composed strategic settlement stages, on a shared 96x72 canvas.
# Roads and fields stay deliberately sparse; no procedural pixel scattering.
def settlement(stage, fortress=False):
    im=Image.new('RGBA',(96,72));d=ImageDraw.Draw(im)
    def rect(b,c):d.rectangle(b,fill=col(c))
    def poly(points,c):d.polygon(points,fill=col(c))
    def path(points,width=3):d.line(points,fill=col('road'),width=width)
    def house(x,y,w=10,h=8,roof='tile'):
        # Rectangular pitched roofs with a short visible gable, not conical huts.
        # Broad material clusters, sparse framing, no random texture/noise.
        left,right=x-w//2,x+w//2
        poly([(left-1,y-1),(right+3,y-1),(right+1,y+2),(left,y+2)],'earth')
        rect((left,y-h,right,y-1),'plaster')
        rect((right-2,y-h,right,y-1),'soil')
        poly([(left-2,y-h+1),(left+2,y-h-5),(right-1,y-h-5),(right+3,y-h),(right+1,y-h+3),(left,y-h+3)],'roof')
        poly([(left-1,y-h),(left+2,y-h-4),(right-1,y-h-4),(right+1,y-h+1),(left,y-h+1)],roof)
        d.line((left+2,y-h-5,right-1,y-h-5,right+2,y-h),fill=col('light'))
        d.line((left,y-h+1,right,y-h+1),fill=col('earth'))
        rect((left,y-h+3,left,y-1),'earth')
        rect((right-3,y-h+3,right-3,y-1),'earth')
        rect((x-1,y-4,x+1,y),'earth')
        if w>=10:
            rect((left+2,y-5,left+3,y-4),'ink')
            d.point((left+2,y-5),fill=col('ivory'))
        if w>=14:
            rect((x+4,y-5,x+5,y-4),'ink')
            rect((right-3,y-h-7,right-2,y-h-3),'stone')
            d.point((right-3,y-h-7),fill=col('wall'))
        # One eave glint, not a stippled/dithered roof.
        d.line((left+2,y-h,right-3,y-h),fill=col('light'))
    def tower(x,y):
        rect((x-4,y-10,x+4,y),'steel')
        rect((x-3,y-9,x+2,y-1),'stone')
        rect((x-3,y-9,x-2,y-1),'wall')
        rect((x-4,y-11,x+4,y-9),'stone')
        for dx in (-4,0,3):rect((x+dx,y-13,x+dx+1,y-11),'wall')
        rect((x,y-7,x,y-5),'ink')
        d.line((x-3,y-2,x+2,y-2),fill=col('steel'))
    def wall(points):
        d.line([(x+1,y+2) for x,y in points],fill=col('earth'),width=5)
        for (x,y),(xx,yy) in zip(points,points[1:]):
            poly([(x,y),(xx,yy),(xx,yy-6),(x,y-6)],'stone')
            d.line((x,y-6,xx,yy-6),fill=col('wall'),width=2)
    def field(x,y,w=15,h=8):
        rect((x,y,x+w,y+h),'soil')
        for yy in range(y+1,y+h,3):
            for xx in range(x+1,x+w-1,4):
                d.line((xx,yy+1,xx,yy-1),fill=col('gold'))
                d.point((xx+1,yy),fill=col('grain'))
    if fortress:
        # A compact garrison, visibly distinct from a walled civilian city.
        path([(48,66),(48,52),(47,31)],4)
        poly([(25,28),(59,25),(77,39),(73,56),(35,59),(22,46)],'soil')
        wall([(23,32),(27,23),(62,23),(78,36),(75,54)])
        house(39,41,19,9,'roof'); house(63,44,16,8,'roof')
        rect((39,19,54,30),'steel'); rect((39,19,51,29),'stone')
        rect((39,18,53,20),'wall')
        for dx in range(39,54,4): rect((dx,16,dx+1,18),'wall')
        rect((45,24,48,30),'ink')
        tower(38,30); tower(56,30)
        # Banner is the same restrained blue used by the military interface.
        rect((48,8,48,16),'earth'); poly([(49,8),(56,9),(53,13),(49,12)],'blue')
        wall([(23,32),(22,49),(39,58)]); wall([(52,58),(75,54)])
        for x,y in [(27,23),(62,23),(78,36),(22,49),(39,58),(52,58),(75,54)]:tower(x,y)
        return im
    path([(7,49),(29,40),(43,41),(57,52),(90,52)],4)
    path([(38,12),(39,24),(43,41),(37,63)],3)
    field(11,38,12,8)
    houses=[(30,30,9,7),(54,28,9,7),(43,45,17,10),(25,52,9,7),(58,56,9,7)]
    if stage>=1:
        path([(19,22),(39,24),(68,33),(74,50)],3)
        path([(17,56),(29,59),(37,63),(63,64)],3)
        houses += [(18,25,9,7),(68,35,10,8),(75,50,9,7),(40,64,9,7),(65,66,9,7)]
        field(57,12,14,8);field(9,57,12,8)
    if stage>=2:
        path([(26,18),(53,18),(70,26),(77,40)],3)
        wall([(17,29),(22,14),(58,12),(81,26),(85,53)])
        houses += [(32,17,8,6),(51,18,8,7),(77,30,8,6),(13,45,8,6),(56,41,9,7),(71,61,9,7)]
    for x,y,w,h in sorted(houses,key=lambda p:p[1]):
        house(x,y,w,h)
    if stage>=1:
        # Market awnings: simple alternating canvas panels, not noisy detailing.
        for x,y in ((52,50),(64,46)):
            rect((x-4,y-3,x+4,y),'earth')
            for dx in range(-4,5,3):rect((x+dx,y-5,x+dx+1,y-3),'ivory' if dx%2 else 'blue')
    if stage>=2:
        # Stone keep and foreground curtain walls.
        rect((38,26,51,37),'steel');rect((38,26,47,37),'wall')
        tower(37,38);tower(53,38);rect((43,32,46,37),'ink')
        wall([(17,29),(15,53),(33,63)])
        wall([(44,64),(66,68),(85,53)])
        for x,y in [(22,14),(58,12),(81,26),(15,53),(33,63),(44,64),(85,53)]:tower(x,y)
    return im
for stage,name in enumerate(['settlement','town','city']):save(settlement(stage),name+'.png')
save(settlement(2,True),'fortress.png')
print('Exported',len(list(OUT.glob('*.png'))),'palette-exact source sprites')
