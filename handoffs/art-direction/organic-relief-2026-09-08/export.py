"""Convert selected built-in concepts to the canonical runtime grid/palette."""
from pathlib import Path
from PIL import Image, ImageDraw

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
DEST=ROOT/'assets/sprites/organic-relief'
DEST.mkdir(parents=True,exist_ok=True)
approved={tuple(bytes.fromhex(s[1:])) for s in (ROOT/'config/art-palette.hex').read_text().splitlines() if len(s)==7}
earth=['74513F','4E3B39','716D70','596679','394658','886044','49352F']
greens=['235747','337A58','193E42']
palette=[tuple(bytes.fromhex(c)) for c in earth+greens]
assert set(palette)<=approved
assets={'hill-a':('hill-1',(64,32)), 'hill-b':('hill-2',(64,32)),
        'ridge-a':('ridge-1',(64,40)), 'ridge-clean':('ridge-2',(64,40)),
        'canopy-a':('canopy-1',(16,12)), 'canopy-b':('canopy-2',(16,12))}
for source,(name,size) in assets.items():
    im=Image.open(HERE/'source'/f'{source}.png').convert('RGBA')
    alpha=im.getchannel('A').point(lambda a:255 if a>=200 else 0)
    bounds=alpha.getbbox()
    assert bounds and alpha.getextrema()[0]==0, f'{source}: missing real transparency'
    im=im.crop(bounds).resize(size,Image.Resampling.NEAREST)
    out=Image.new('RGBA',size)
    colors=palette if 'canopy' not in name else [tuple(bytes.fromhex(c)) for c in greens]
    for y in range(size[1]):
        for x in range(size[0]):
            r,g,b,a=im.getpixel((x,y))
            if a<200:continue
            color=min(colors,key=lambda c:(r-c[0])**2*.3+(g-c[1])**2*.59+(b-c[2])**2*.11)
            out.putpixel((x,y),(*color,255))
    out.save(DEST/f'{name}.png')
    if 'canopy' not in name:
        # Vegetated parts inherit the climate material. Rock remains shared.
        for biome,replacements in {
            'plain':['337A58','49975B','235747'],
            'forest':['235747','337A58','193E42'],
            'jungle':['193E42','235747','193E42'],
            'taiga':['235747','4F8C7A','193E42'],
            'desert':['74513F','886044','49352F'],
            'tundra':['716D70','A78D72','4E3B39'],
        }.items():
            variant=out.copy()
            remap={tuple(bytes.fromhex(a)):tuple(bytes.fromhex(b)) for a,b in zip(greens,replacements)}
            for y in range(size[1]):
                for x in range(size[0]):
                    c=out.getpixel((x,y))
                    if c[3] and c[:3] in remap:variant.putpixel((x,y),(*remap[c[:3]],255))
            variant.save(DEST/f'{name}-{biome}.png')
rows=[]
for name,size in assets.values():
    root='world.canopy.'+name[-1] if 'canopy' in name else 'world.relief.'+name.replace('-','.')
    rows.append(f'{root} organic-relief/{name}.png {size[0]/16} {size[1]/16} 0 0 0 0')
    if 'canopy' not in name:
        for biome in ['plain','forest','jungle','taiga','desert','tundra']:
            rows.append(f'{root}.{biome} organic-relief/{name}-{biome}.png {size[0]/16} {size[1]/16} 0 0 0 0')
catalog=ROOT/'assets/sprites/sprites.catalog'
text=catalog.read_text(); marker='# Organic relief and forest canopy'
text=text.split(marker)[0].rstrip()+'\n\n'+marker+'\n'+'\n'.join(rows)+'\n'
catalog.write_text(text)
sheet=Image.new('RGB',(768,360),'#121418'); labels=ImageDraw.Draw(sheet)
for i,(name,size) in enumerate(assets.values()):
    im=Image.open(DEST/f'{name}.png'); scale=min(4,220//im.width,140//im.height)
    im=im.resize((im.width*scale,im.height*scale),Image.Resampling.NEAREST)
    x=(i%3)*256;y=(i//3)*180
    sheet.paste(im,(x,y+24),im);labels.text((x+4,y+4),name,fill='white')
(HERE/'previews').mkdir(exist_ok=True)
sheet.save(HERE/'previews/exports.png')
print('Exported 30 exact-palette sprites, 6 silhouettes with climate vegetation variants')
