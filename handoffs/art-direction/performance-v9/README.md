# Rendering performance patch — 2026-09-07

Active project: C:/Paladin, C++/SDL. Updated executable: C:/Paladin/out/build/x64-Debug/Paladin.exe.

## Result

The largest regressions were per-frame road/foundation rasterization, linear object lookups inside those rasterizers, detailed trees bypassing their distance cutoff, and SDL's software backend scaling a whole map before clipping it to the window.

Reproducible 1280×720 software-rendered x64-Debug fixture: 256×256 map, 64 houses, 384 road tiles, 1,664 trees, plus shallow/deep coastal water. Camera is centered on the housing district. These are rendering measurements without citizen simulation, not an in-game FPS guarantee. Warm measurements average two frames after three warm-up frames; the first frame also loads the sprite library. Hardware, renderer backend and scene contents affect timings.

| Screen pixels per tile | Before, warm ms | After, warm ms | Before submitted items | After submitted items |
| --- | ---: | ---: | ---: | ---: |
| 4 | 448.766 | 15.758 | 19,544 | 576 |
| 8 | 430.475 | 23.339 | 16,380 | 576 |
| 16 | 4,227.260 | 30.344 | 52,828 | 500 |
| 32 | 2,112.750 | 40.331 | 23,677 | 596 |
| 64 | 1,779.750 | 53.871 | 6,453 | 210 |

At four pixels per tile, first-frame time also fell from 577.881 ms to 103.273 ms. Item counts describe the raised scene queue; cached terrain and lighting composites are additional draws.

## Presentation and caching

- Below 16 screen pixels per tile, terrain uses a single cached overview sampled from authored colors. Trees use compact static overview chunks. Below four pixels, individual natural-feature markers are omitted and terrain communicates the biome.
- Detailed terrain, modular trees, ground transitions and minor building details start at 16 pixels per tile. Roof fringe, grass, doors, chimney smoke, logging effects and water animation require 24 pixels per tile. Sprite animation strips also use their first frame at distant zoom. Thresholds live in src/rendering/SceneDetail.h.
- Nearby animations retain the existing pause-aware presentation clock. Camera distance does not change production, citizen movement, fuel consumption, tree regrowth, collision or simulation progress.
- Road contours and foundation fades are composed into textures and reused. Cache construction is budgeted to twelve entries per frame, with a roughly 32 MiB retention budget and 512-pixel maximum texture side. Building sprites remain available while optional ground detail is prepared.
- Road caches validate their local neighboring tiles after topology changes. Distant placement preserves existing edges; adjacent placement refreshes the affected contour. Foundation caches depend on the building footprint. Art reload and map changes invalidate these caches.
- Tile occupancy lookups now use lazy spatial indexes invalidated by topology changes, replacing reverse scans through the entire object list.
- Buildings are culled against their artwork extents before presentation work. Logging decorations traverse the visible part of their work area. Forest caches release distant chunks instead of accumulating textures for the whole map.
- Night lighting reuses its light field while inputs are unchanged. Nearby lamp flicker updates at ten samples per second; distant lamp illumination stays steady. Cool ambient light and warm local illumination remain separate from authored palette colors.
- Texture source and destination rectangles are clipped before SDL scales them, including overview maps at close zoom. Premultiplied compositing preserves the opacity of cached ground blends.

No source PNGs or palette files were changed. F8 artwork toggling, F6 reload, whole-wall roof cutaways and replaceable artist exports remain available.

## Verification

- x64-Debug build passed; game and smoke executables rebuilt and runtime assets packaged.
- Complete Paladin core tests passed.
- Complete application rendering/UI smoke checks passed, including authored terrain at multiple zoom levels, art toggling/reload, day/night lighting, roof cutaways, water, doors, heating smoke and logging animations.
- New rendering regression checks verify unchanged distant images as animation time advances, a distinct night image, fewer than 2,000 submitted items in the distant fixture, zero submitted items when the entire settlement is off-screen, and local road-cache invalidation.
- A pixel comparison verifies that cached translucent patches match direct compositing rather than darkening twice.
- Reviewed distant-city, nearby heated-home, coastline and nighttime tree-clearance captures. Captures are in previews/ beside this document.
- Rendering performance checks are registered as PaladinRenderPerformanceTests when PALADIN_BUILD_APPLICATION_SMOKE_TESTS is enabled. Timing values are diagnostic; correctness and draw-work budgets are asserted instead of machine-specific timing limits.

Build and test logs: C:/Paladin/out/performance-build.log, performance-core.log, performance-smoke.log, performance-before.log and performance-after.log.
