# Coast, building and world-art revision

Runtime exports are in `assets/sprites/modular-v1/` and `assets/sprites/world-biomes/`. Selected built-in image-generation sources are in [source](source/); the complete generation prompt set, including iterations, is in [prompts.md](prompts.md). The approved palette and canonical final grid are unchanged: 64 palette entries, binary alpha, 16 art pixels per logical tile. Source concepts are larger; they are not used directly by the renderer.

## Art and catalog composition

- `wall.adobe` is a shared tan-brown plaster family with worn plaster patches, a sill and a substantial base. Doors, shutters and hangings are separate attachments. House, bakery and keep select the same wall/roof families through `config/city-objects.catalog`.
- `roof.thatch` has bound/repaired and covered-dormer variations. All variations fit the same occupied map area. Rotation only changes orientation, never the building footprint or navigation.
- The optional final object-style field selects a shared decoration pool. `pieces.catalog` accepts optional `choices choice` fields for deterministic assortment; omitted fields retain existing behavior. A missing pool or sprite leaves that decoration absent. Object-specific pieces coexist with shared pools; the stockpile's unique crate stays specific to the stockpile.
- The bed retains its existing `.72 × .92` tile dimensions. The keep receives a long woven carpet, warm plank floor, communal table and grounded storage details. These interior pieces remain separate from shell materials.
- Nine world-only terrain exports use restrained subsets of the approved palette. Warm biome tiles no longer alias one another through city temperature variants. Mountains and hills keep their existing artwork. Political overlays and future roads/settlements can use the quieter base.

## Rendering fixes

The construction pulsation was a presentation-cache bug. A road's local dependency signature changes when neighboring construction finishes. Rebuilding its texture reset `readyAt`, restarting a 180 ms wall-clock fade and crossfade to a rectangular road proxy. Repeated completions made the contour appear and disappear. Cached surfaces now submit at full opacity immediately. Local invalidation, bounded rebuilding and eviction are retained; cache misses use the existing contour renderer. Ground caches now use the canonical 16-pixel tile resolution.

Both coast renderers sample a small deterministic displacement in map coordinates. City shores use more variation and animated shallow foam; world shores use smaller variation and a narrow sediment band. The dark grass coast outline is replaced with a vegetation midtone. Animation continues to use presentation time, which freezes while paused.

Tall fitted roofs also exposed gaps where separately rasterized animated eave strips met. The roof now keeps its intact base under the loose moving fringe, with whole source rows for the animation strips. A pixel-render regression checks the opaque center for background cracks across multiple animation times.

Three shared time buttons replace the four-button row. The first toggles pause/normal speed and displays the pause symbol while paused; the other tiers retain their existing behavior. The same component serves city and world screens.

## Planet preparation

The current world view remains flat as requested for this preparation step. `WorldSurface` supplies sphere sampling, yaw/pitch rotation, inverse UV lookup and circular-disc picking with sky rejection. A future planet camera can rotate the surface and pick its visible hemisphere without using Mercator panning. This PR does not add a globe UI, alter simulation topology, or connect longitude seams in pathfinding.

## Rebuild and review

Run `powershell.exe -NoProfile -ExecutionPolicy Bypass -File handoffs/art-direction/coasts-modular-2026-09-07/export.ps1 -Project <checkout>`. It reuses the existing nearest-neighbor/palette exporter for objects. World material conversion integrates each source area before palette quantization to suppress source flecks at strategic scale. Neither changes the artist's masters. `verify.py` audits every runtime export's dimensions, alpha and exact palette membership, generates a contact sheet and converts smoke-review BMPs to PNGs.

Build and run `PaladinArtCheck` before delivery. For screenshots, set `PALADIN_SMOKE_SCREENSHOTS` to this folder's `previews` directory and `PALADIN_ART_REVIEW=1`. Run the ordinary CTest suite as well.

Regression checks cover immediate/stable cache opacity, unrelated versus neighboring construction invalidation, distinct world biome sources, roof dimensions, planet rotation/picking and shared time-control behavior. Mandatory art checks inspect the final day/night pixel blocks and resource cutouts. Review PNGs include normal and close city views, interiors, night, coasts and a six-biome world strip at equal temperature.

Verified on Windows Release: `PaladinArtCheck`, all four CTest entries, the 25-export palette/alpha/dimensions audit, and native city pause/resume interaction. Final software-renderer pan/zoom measurements are recorded in `artcheck.log` and `gallery.log`. Missing/off-palette asset messages in the smoke log are intentional negative fixtures.
