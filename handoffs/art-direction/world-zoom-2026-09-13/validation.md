# World zoom performance investigation — 2026-09-13

Request: eliminate world-planet zoom freezes while preserving the existing rendering, territory, pixel detail and dynamic solar optics.

## Reproducible native baseline

The Game Development Studio performance skill was consulted. Its separately installed `game-dev` CLI and a sealed adapter are unavailable on this machine, so these are native application measurements, not a sealed game-dev performance admission.

Actual Debug `Paladin.exe`, SDL Direct3D 11, vsync disabled, 1280×720, seed 73517, 600×440 world, 37 realms, 142 settlements. Each pass contains 180 logarithmically spaced zoom frames from 0.6 to 60 and back. Initial terrain preparation completes before timing; political pages start cold. Full application layout, rendering and presentation are timed. Capture readback is outside the timed interval.

| Baseline pass | Mean ms | p95 ms | Worst ms | Frames >50 ms |
|---|---:|---:|---:|---:|
| Political, cold | 20.11 | 68.16 | 1404.82 | 13 |
| Political, return | 7.59 | 12.51 | 68.76 | 1 |
| Terrain only | 4.57 | 10.35 | 13.07 | 0 |

The first political presentation consumed 1158.82 ms of CPU submission time. Detail-page construction consumed 52–83 ms at several zoom boundaries. Separate sphere geometry/lighting submission reached 8–18 ms at those views. The sun was already cached and was not the source of these stalls.

## Bounded candidate

Primary target: p95 ≤16.67 ms for both political sweeps in the comparable native Debug scenario. Guardrails: no >50 ms zoom frames; preserve completed pixel output, coast masks, native markers, lighting and 16-art-pixel detail; retain the 32 MiB resident political texture cap. Limit this investigation to three measured candidate iterations, revisiting the diagnosis if none passes.

Changes are restricted to world political preparation/rasterization, exact globe vertex reuse, demographic influence invalidation, the renderer frame counter, world loading integration, their regression checks and the optional application benchmark. No source art or visual quality settings are reduced. Native benchmark and art-test evidence is retained here and under `.cache/world-zoom-*`.

Initial coarse political preparation now belongs to the existing world-loading phase and is measured separately. Runtime updates and cold detail pages use bounded work across frames; the last completed nonstructural presentation remains visible until replacement commits. Topology changes discard invalid old political ink immediately. Population bookkeeping that does not change the actual resident count no longer invalidates the influence field.

## Final native Debug measurements

Same baseline seed, camera sweep, viewport and Direct3D 11 path:

| Final 1280×720 pass | Mean ms | p95 ms | Worst ms | Frames >50 ms |
|---|---:|---:|---:|---:|
| Political, cold | 7.54 | 12.88 | 16.34 | 0 |
| Political, return | 8.76 | 15.13 | 17.75 | 0 |
| Terrain only | 4.71 | 11.36 | 13.04 | 0 |
| Fractional population updates | 8.30 | 14.11 | 18.02 | 0 |
| Four real population updates | 10.75 | 18.31 | 25.13 | 0 |

The cold political sweep's p95 fell 81%, and its worst frame fell from 1404.82 to 16.34 ms. The combined cold/return political sweeps contain no frame over 50 ms (baseline: 14). Warm mean and p95 are slightly higher than baseline because new detail work is distributed into more frames instead of completing in a few blocking frames; the large hitches are removed.

Additional 1920×1200 validation on the same machine:

| Final wide pass | Mean ms | p95 ms | Worst ms | Frames >50 ms |
|---|---:|---:|---:|---:|
| Political, cold | 7.18 | 13.07 | 21.25 | 0 |
| Political, return | 7.40 | 12.73 | 23.74 | 0 |
| Terrain only | 3.68 | 8.82 | 10.50 | 0 |
| Fractional population updates | 6.58 | 11.65 | 18.13 | 0 |
| Four real population updates | 9.20 | 15.23 | 22.58 | 0 |

Each live scenario uses the real strategic population system. The fractional sweep changes bookkeeping every frame but causes zero resident/influence changes. Four annual migration ticks add 568 residents across 142 settlements and cause exactly four real influence revisions. These are deliberately accelerated validation scenarios, not normal gameplay time cadence.

Initial complete terrain plus political preparation takes about 6.6–7.3 seconds for this Debug world, outside interactive frame timing and inside the existing loading phase. At 1280×720, all five requested stationary captures are already settled after one frame following the sweeps. At 1920×1200, the regional view requests the full accepted 64-page resident set and settles in 168 frames (2.36 seconds including PNG capture); all other capture views settle in one frame. Pages retain full canonical resolution; more distant pages beyond the existing 32 MiB cap use the existing coarse presentation.

Three candidate implementations were investigated. Validation exposed and corrected two scheduling defects: globe and tangent views cancelling one another's partial pages, and visibility scanning consuming the detail-work timer before any raster rows could progress. The final budget measures actual cache work only. The compact source also reduced a previous 12.95-second transition detail wait to 1.89 seconds before the scheduling correction removed that remaining wait at the default viewport.

The benchmark's initial preparation loop now presents each loading frame, matching interactive play. An intermediate 14.9-second first-present result was a benchmark defect caused by queued, unpresented clear commands, not the final game path. Settled captures now read before `Present`, as required by SDL's backbuffer lifetime contract. Earlier in-sweep captures use identical post-present routes for relative baseline comparison and may depict an earlier swap-chain buffer; do not treat their file indices as exact current-frame camera positions.

## City guard

Actual Debug executable, 576×576 city, continuous zoom and fast scrolling at 1× and 5×: zoom p95 8.94/10.62 ms; scrolling p95 4.19/4.04 ms. The first cold city zoom frame is 211 ms, retained in the measurement, and is outside this world-zoom fix. Subsequent 5× zoom worst frame is 17.08 ms. No city rendering algorithms or source art changed.

## Final rendering verification

Both Release and Debug executables rebuilt successfully. `Play Paladin.cmd` continues to launch `out/build/x64-Release/Paladin.exe`.

- `PaladinTests`: passed after the final header and invalidation changes.
- `PaladinArtCheck`: passed, including `PaladinWorldSurfaceTests` and the full application routing/day/night smoke checks.
- 102,400 canonical surface samples match the original world-backed sampler exactly, including civic ownership, all interpolation weights, primary/secondary tribal influence, longitude wrapping and polar/out-of-range inputs. Additional checks verify immutable old snapshots and correct new snapshots after terrain, resident-count and realm-origin changes.
- Repeated population changes cannot starve a refresh; once settled, the resulting entire raster matches a newly built renderer. Blended globe/tangent detail preparation completes. Political detail textures remain within 32 MiB.
- Coast mask: zero political ink on water; 9,979 painted land samples. All four flat/globe and terrain/political composites contain no unintended black pixels. All 480 marker-motion captures preserve identical translated annotation plates.
- Dynamic sun still changes 23,011 pixels over the animation interval, holds exactly while paused, and keeps its texture cache unchanged.
- Representative final PNGs are byte-identical to the pre-change approved review: coast mask, all four full world composites, whole/regional/polar globe captures, both dynamic sun time samples, normal day/night city views, close day/night house views and close city coast. Normal and close day/night views, wide regional territory, rolled political coast and solar-limb images were also inspected visually.

The first full art run passed world-surface checks but exceeded the existing software globe timing limit (worst 103.75 ms). A quiet rerun passed unchanged, mean 43.04 ms and worst 79.56 ms against the unchanged 80 ms limit. Both logs are retained. Hardware-accelerated native application measurements above are separate from this software-renderer timing guard.

`git diff --check` passed. No runtime sprite, palette, source-art, simulation behavior or visual-quality settings were reduced. The new benchmark supports PNG capture in source-art builds and BMP capture in packaged builds without adding an SDL_image runtime dependency to the packaged game.
