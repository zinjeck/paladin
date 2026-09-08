# Paused animation, zoom continuity and city coasts

Active project: C:/Paladin. Restart out/build/x64-Debug/Paladin.exe for the code changes. F8 still toggles environment artwork, F6 reloads exports and O switches roofs.

## Animation and zoom

City and world renderers now receive a shared presentation clock from SimulationClock. It advances only while unpaused. Roofs, trees, grass, doors, water and light flicker all use this clock. Cosmetic animation keeps its natural pace at faster simulation speeds, and does not catch up for time spent paused.

Terrain chunks keep usable textures across zoom-resolution changes. Newly visible chunks are populated immediately; existing chunks can retain their previous resolution while refreshing. Mixed resolutions share a 64 MiB texture budget. Strategic zoom retains its simpler overview, and water detail animates where a tile occupies at least eight screen pixels.

Interior floors and rear/side fallback wall strips no longer protrude around full roof sprites. Floors and wall caps remain available in cutaway view. The supplied keep close-up was checked against the reported exposed-floor problem.

## Ground transitions

An irregular translucent soil apron blends object footprints and roads with surrounding land. It uses the existing road material, sits beneath floors and shadows, and is omitted on surrounding water or surfaces that already match the grass. This is a presentation blend; exported art pixels remain within the palette.

## Water and beaches

Deep water uses Royal blue #3F5F9A with sparse Azure cloth #548AC4 glints. Shallows use #548AC4, Aether cyan #63BFC3 and Ice blue #AFC9D6. Three animation frames and staggered module phases keep water moving without the previous dense scribbled pattern. Both city and world water use the new art; city water distinguishes depth by distance from land.

SettlementGrid now has a separate CityTileType: Inland, Beach, Coast, ShallowWater and DeepWater. Beach remains walkable Land for existing construction and movement rules. Strategic WorldGrid terrain is unchanged.

City map generation classifies a one-tile sandy fringe on suitable gentle, low, non-cold coasts. Seeded patches prevent every coast becoming a beach. Steep or mountain-adjacent coasts retain their original terrain. Water within three tiles of land is shallow; farther water is deep. Beach tiles use the new sand sprite, blend with neighboring grass, and have a wet shore edge. Other coasts have a dark bank edge and sparse wavelets. New natural-feature generation avoids placing trees on beach tiles. Classification happens when a city map is generated.

## Files and checks

- Game exports: C:/Paladin/assets/sprites/environment-v7.
- Generation sources, exact prompts and export.ps1 are in this directory; the built-in image_gen tool was used. The first busy water source is retained as a rejected reference.
- Active recipes: C:/Paladin/assets/sprites/sprites.catalog. The preceding catalog is retained in catalog-before.txt.
- Previews: previews/coast-day, coast-paused, coast-moving, coast-zoom, keep-close and the existing day/night/cutaway galleries.
- Palette audit: active-sprite-verification.csv. All 47 active PNGs have zero off-palette visible pixels and zero partial-alpha pixels.

Core tests and full software-rendered application smoke checks pass. New coverage checks the paused clock, first-frame artwork across multiple zoom levels and uncached terrain regions, and the beach/coast/shallow/deep classifications. Paused coastal captures are byte-identical; the moving capture differs. Coast, foundation and keep close-up previews were visually reviewed.
