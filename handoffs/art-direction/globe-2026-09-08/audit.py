"""Audit final packaged assets and grass coverage; run after the local builds."""
from pathlib import Path
from collections import Counter
import hashlib
import json
from PIL import Image

root = Path('C:/Paladin')
folder = Path(__file__).parent
report = {}
for config in ('x64-Debug', 'x64-Release'):
    build = root / 'out/build' / config
    files = list((root / 'assets/sprites/organic-relief').glob('*.png'))
    files.append(root / 'assets/sprites/sprites.catalog')
    for source in files:
        packaged = build / source.relative_to(root)
        assert packaged.read_bytes() == source.read_bytes(), str(packaged)
    exe = build / 'Paladin.exe'
    report[config] = {'executable': str(exe), 'sha256': hashlib.sha256(exe.read_bytes()).hexdigest(), 'matching_packaged_assets': len(files)}

im = Image.open(folder / 'organic-city-close.bmp').convert('RGB')
fractions = []
for y in range(64, im.height-64, 64):
    for x in range(64, im.width-64, 64):
        counts = Counter(im.crop((x,y,x+64,y+64)).getdata())
        fraction = 1 - counts.most_common(1)[0][1]/4096
        assert fraction > .08, (x,y,fraction)
        fractions.append(fraction)
report['grass'] = {'minimum_non_dominant_fraction_per_64px_patch': min(fractions), 'maximum': max(fractions), 'patches_checked': len(fractions)}
tiles = [im.crop((x,y,x+32,y+32)).tobytes() for y in range(64,640,32) for x in range(64,1216,32)]
assert len(set(tiles)) == len(tiles), 'Repeated exact grass tile'
report['grass']['distinct_32px_regions'] = len(set(tiles))
(folder / 'local-builds.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
