# Paladin: cohesive tribal sprites and gradual zoom presentation

Active project: `C:/Paladin` (C++/SDL). This supersedes the detail cutoffs in `../performance-v9/README.md`.

## Artistic direction

Mud means attractive earthen construction, not patchwork, stains, or ruined plaster. Enclosed tribal buildings share clean brown mud walls and a continuous brown thatch roof, with restrained wear, clear shaded planes, simple wooden doors, and unglazed windows. No yellow roof tiers or elaborate architectural ornament. Each expanded blueprint receives one fitted roof.

All catalog sprites now pass through one shared import treatment in `src/rendering/SpriteStyle.h`: at most 24 source texels per logical tile, nearest sampling, and one local palette-preserving cleanup of isolated texture marks. This is a presentation density, not a claim about “24-bit” art. Every category uses the same treatment, including terrain, foliage, crops, props, resources, buildings, and entity sprites. Small originals are not enlarged. Frames are processed independently. Strong edges and transparency remain discrete; no blur or invented intermediate artwork colors. Texture sharing includes the manifest dimensions so a tiny wall cap cannot accidentally determine a whole facade's resolution.

This is deliberately a moderate simplification. Earlier trial previews at 16 texels with repeated cleanup melted structural details; that treatment is not the final standard. Artist originals and Krita projects remain untouched. The old `pixel-density.txt` per-folder sampling rule is superseded by this shared importer.

## Assets and reproduction

- Final generated source: `source/tribal-clean-atlas.png`.
- Grass source: `source/grass.png`.
- Earlier `tribal-atlas.png` and `tribal-simple-atlas.png` are trial references, not the active export source.
- Runtime exports: `C:/Paladin/assets/sprites/tribal-v10/` and entries in `assets/sprites/sprites.catalog`.
- Run `powershell.exe -NoProfile -ExecutionPolicy Bypass -File handoffs/art-direction/tribal-v10/tools/export.ps1` from the project root. Windows PowerShell 5 supplies the System.Drawing runtime used by this exporter.
- Run `tools/audit.ps1` the same way to check every catalog-referenced PNG against the master palette and binary transparency.

The exporter crops the source sheet, keys the magenta surround, exports nearest-sampled PNGs, and maps earth materials only to the chosen earth/wood subset of the existing 64-color palette. This prevents orange fire colors from becoming plaster or straw. Grass uses the palette's green family, with broad quiet patches and warm/cold/jungle variants. Source roof is 96×88, rotated side roof 88×96, facades 96×28, and ground/material tiles 32×32; the importer applies the common display-detail cap afterward.

Generated using the built-in ImageGen tool with existing roof artwork as a reference. Final source was copied from `C:/Users/Super/.codex/generated_images/01a077eb-cbcd-7473-9710-020251c2b278/exec-2119159f-0820-43f8-a696-d2d498697c38.png`. Prompt direction: preserve the four sprite positions; remove wall patches, holes, and cracks; coherent hand-plastered earth, simple doors/windows, restrained wear; continuous hipped brown thatch with clear planes and sparse broad straw marks; no grain, dithering, yellow roof tiers, or ornate details. The full generation prompt remains in the task tool history. Generated source images are references; verified runtime PNGs, not raw generated RGB, are the palette-compliant game assets.

## Rendering behavior

- Natural features remain visible at maximum distance using maintained tree/rock density summaries, then blend into static caches made from the actual modular sprite variants. They blend into individually sorted sprites between 14 and 28 screen pixels per tile. Wind strength ramps in from 24 to 36; the pause-aware simulation presentation clock still controls world animation.
- Static natural-feature caches cover 16×16 tiles at eight texels per tile. At most two are built per frame, with a two-millisecond scheduling deadline checked between jobs. A job already started may exceed that time. Cache retention is capped at 256 textures (about 16 MiB). Terrain uses fixed 16×16-tile, 256×256-pixel caches with a 64 MiB cap, nearest-first scheduling, and no resolution rebuild on zoom. A stable nearest working set prevents far-view cache churn.
- New cache content fades in over 180 ms. Terrain detail blends over 2–10 screen pixels per tile. Road contours and foundation transitions blend over 16–24 and fade in as caches arrive. The overview/road base remains available during preparation. These are camera/loading presentation transitions, not advancing world animations while paused.
- Feature summaries follow harvest, regrowth, and clearing. Cache invalidation includes neighboring chunks touched by tree crowns and building navigation changes. Empty harvested chunks discard old textures. Tree clearance matches the single roof's overhangs.
- Front, back, left, and right building views follow the entrance side, including placement previews. World entrance heading is separate from camera heading in `BuildingView.h`; additional angular art samples can be added later. This does not implement a rotating camera or full 360-degree building art.
- Stockpiles show a modest shelter, an open timber yard, and stacks for inventory actually held by that stockpile. Empty inventory shows no fake goods. Stack counts are a bounded visual summary (up to four per resource, 48 total), not one sprite per item.
- The goods panel opens on the right by default. Grass variation uses the settlement's world-derived temperature and biome, including latitude effects; local screen Y is not treated as latitude.
- F8 artwork toggle, F6 reload, roof cutaways, warm local night lights, cool night ambience, shadows, and existing heating/logging simulation remain available. Authored palette rules are separate from dynamic lighting and opacity.

## Verification

The `PALADIN_ART_REVIEW=1` smoke mode constructs four house facings, an extended keep, bakery, farm, stockpile with empty/filled inventory, trees, and rocks. It writes day/night and zoom captures when `PALADIN_SMOKE_SCREENSHOTS` names a directory. Final previews are in `previews/`.

The regular application smoke suite checks rendering, palette rejection, animation pause behavior, roof cutaways, tree clearance, and application routing. Added checks cover feature summaries after harvest/clear on irregular grid edges, independent animation-frame import, and entrance/camera-relative facing. The benchmark checks distant static rendering, draw-work budgets, offscreen culling, local road invalidation, and continuous zoom through both houses and dense forest.

Build/test logs are in `C:/Paladin/out/tribal-v10-{build,core,smoke,review,performance,palette}.log`. Output logs and compiled binaries belong in `out`; generated artwork and review images belong here.

Final software-rendered x64-Debug benchmark at 1280×720: 256×256 map, 64 houses, 384 road cells, 1,664 trees, and coastal water. Continuous zoom through housing averaged 53.71 ms (worst 76.34 ms); through dense forest averaged 46.81 ms (worst 64.18 ms). Steady-view diagnostic samples at 4/8/16/32/64 pixels per tile were 26.02/46.61/46.32/48.74/61.11 ms after three warm-up frames; caches may still be preparing in these early samples. First view including library import took 155.29 ms. These are rendering-only software Debug measurements, not an FPS promise for the full simulation or GPU renderer. Compared with v9, the distant renderer intentionally retains more visible features; this patch addresses disappearing features and synchronous zoom rebuilds rather than claiming every frame is faster than v9.

The palette audit verified all 42 referenced runtime PNGs (72,820 pixels) use the existing palette with alpha restricted to 0/255. The packaged roof PNG checksum matches the source runtime export.

Final status: x64-Debug build, complete core suite, regular application smoke suite, art-review scenario, and rendering performance regression checks all passed. Reviewed the final in-game day/night captures after the clean-mud and earth-family color corrections. `previews/tribal-day.png` and `tribal-night.png` are lossless format conversions of those captures. No commit or push was made.
