"""Audit runtime cutouts and final rendered terrain, after PaladinArtCheck."""
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw
from collections import Counter
import json

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
palette = {tuple(bytes.fromhex(s[1:])) for s in (ROOT/'config/art-palette.hex').read_text().splitlines() if len(s)==7}
files = sorted((ROOT/'assets/sprites/organic-relief').glob('*.png'))
assert len(files)==30
sheet = Image.new('RGB',(1000,900),'#121418')
labels = ImageDraw.Draw(sheet)
for i,path in enumerate(files):
    im=Image.open(path).convert('RGBA')
    assert {c[3] for c in im.getdata()} == {0,255}, path
    assert {c[:3] for c in im.getdata() if c[3]} <= palette, path
    expected=(16,12) if 'canopy' in path.name else (64,32) if 'hill' in path.name else (64,40)
    assert im.size==expected,path
    x=(i%5)*200;y=(i//5)*150
    labels.text((x+4,y+4),path.stem,fill='white')
    scale=2 if im.width>16 else 6
    im=im.resize((im.width*scale,im.height*scale),Image.Resampling.NEAREST)
    sheet.paste(im,(x+8,y+30),im)
sheet.save(HERE/'previews/all-exports.png')

for name in ['organic-world-close','organic-city-close','organic-forest-founded']:
    im=Image.open(HERE/f'previews/{name}.bmp').convert('RGB')
    pixels=im.load()
    for y in range(0,im.height,2):
        for x in range(0,im.width,2):
            assert pixels[x,y]==pixels[x+1,y]==pixels[x,y+1]==pixels[x+1,y+1],(name,x,y)

before=Image.open(HERE/'previews/organic-forest-before.bmp').convert('RGB')
after=Image.open(HERE/'previews/organic-forest-founded.bmp').convert('RGB')
diff=ImageChops.difference(before,after)
bounds=diff.getbbox()
# The 3x3 clearing includes crowns that overhang their tile by up to .63 tiles.
assert bounds and bounds[0]>=before.width//2-80 and bounds[2]<=before.width//2+80
assert bounds[1]>=before.height//2-80 and bounds[3]<=before.height//2+80

im=Image.open(HERE/'previews/organic-city-normal.bmp').convert('RGB')
base=Counter(im.getdata()).most_common(1)[0][0]
density=[]
for y in range(0,im.height-64,64):
    for x in range(0,im.width-64,64):
        crop=list(im.crop((x,y,x+64,y+64)).getdata())
        density.append(sum(c!=base for c in crop)/len(crop))
assert max(density)-min(density)>.3
result={'exports':len(files),'palette_and_binary_alpha':'pass','canonical_final_2x2_blocks':'pass',
        'founding_only_changes_canopy_near_city':bounds,'city_detail_coverage_range':[min(density),max(density)]}
(HERE/'audit.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
