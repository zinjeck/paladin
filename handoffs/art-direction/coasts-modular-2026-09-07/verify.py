"""Audit converted sprites and preserve compact, lossless smoke review images."""
from pathlib import Path
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
palette = {
    tuple(bytes.fromhex(line.strip().removeprefix("#")))
    for line in (ROOT / "config/art-palette.hex").read_text().splitlines()
    if line.startswith("#") and len(line.strip()) == 7
}
sizes = {
    "adobe-front": (48, 14), "roof-bound": (55, 49),
    "roof-dormer": (55, 49), "roof-bound-side": (49, 55),
    "roof-dormer-side": (49, 55), "hall-floor": (48, 112),
    "communal-table": (32, 16), "timber-door": (8, 11),
    **{f"bed-{i}": (12, 15) for i in range(8)},
}
count = 0
exports = []
for family in ("modular-v1", "world-biomes"):
    for path in sorted((ROOT / "assets/sprites" / family).glob("*.png")):
        with Image.open(path) as source:
            rgba = source.convert("RGBA")
            assert rgba.size == (sizes[path.stem] if family == "modular-v1" else (32, 32)), path
            pixels = rgba.tobytes()
            for offset in range(0, len(pixels), 4):
                r, g, b, a = pixels[offset:offset + 4]
                assert a in (0, 255), (path, "partial alpha")
                assert not a or (r, g, b) in palette, (path, "off-palette", (r, g, b))
        count += 1
        exports.append(path)
assert count == 25, count
for path in (HERE / "previews").glob("*.bmp"):
    with Image.open(path) as source:
        source.save(path.with_suffix(".png"))
sheet = Image.new("RGB", (1120, 1000), "#121418")
labels = ImageDraw.Draw(sheet)
for i, path in enumerate(exports):
    x, y = (i % 5) * 224, (i // 5) * 200
    with Image.open(path) as source:
        rgba = source.convert("RGBA")
        scale = min(6, 208 // rgba.width, 170 // rgba.height)
        rgba = rgba.resize((rgba.width * scale, rgba.height * scale), Image.Resampling.NEAREST)
        sheet.paste(rgba, (x + (224 - rgba.width) // 2, y + 20), rgba)
    labels.text((x + 8, y + 4), path.stem, fill="white")
sheet.save(HERE / "previews/exports.png")
print(f"PASS: {count} sprites; exact palette, binary alpha and expected dimensions")
