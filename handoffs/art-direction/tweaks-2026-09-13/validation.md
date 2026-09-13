# Paladin tweaks — 2026-09-13

Implemented in C:/Paladin. Run Play Paladin.cmd for the updated Release game.

## Changes

- Flat-map return explicitly centers the settlement just exited, preserving projection and zoom.
- Founding resolves selection in the pointer-release handler, without relying on a separate globe-drag press path. Beginning selection clears old pointer captures. Select Region retains the chosen city/fortress type. Regression coverage uses a site valid for a fortress but invalid for a city, then submits a real map click and naming confirmation. City founding is also exercised on the globe.
- Civic settlement cores use varied curved footprints instead of filled rectangles. Nearby same-realm settlements connect through bounded least-cost land catchments: maximum straight-line separation 24 world tiles, maximum route cost 1.6 times separation plus 2. Mountain/hill resistance, water, rival settlement cores and existing rival civic control constrain routes. Longitude wraps. Remote settlements remain independent. Civic conversion also establishes connections.
- Tribal influence uses the same land connections, with strength limited by the weaker settlement's population-derived authority. Connection geometry is cached across population-only changes and invalidates when settlement geometry/ownership or terrain changes. Rendering does not mutate territory.
- Four ruler spectrum meters replace numerical trait labels. The first pole is on the left; a stronger first trait moves its marker left. Numeric trait data remains in the model. This is a presentation change, preserving the existing AI simulation rather than introducing warfare/trade AI.
- Environment presentation time advances by real elapsed frame time times selected speed (1, 2, 3, 5), and stops when paused. Trees, grass, fitted roofs, water, weather and flickering lights already consume this shared clock. There are no extra per-object animation update loops or texture allocations for speed changes.
- Solar photosphere clipping uses the solid planet silhouette and exact circular-disc visible area. The uncovered disc remains bright. Cached camera bloom/diffraction contracts toward the visible crescent and can spill across the planet edge. Direct glare is zero after full cover. A bounded native-resolution atmospheric rim persists near contact. Optical response and atmospheric scattering are photographic approximations, not a full spectral light-transport solver. Reference: https://www.nasa.gov/image-article/suns-first-rays-peek-above-earths-limb/ .

## Validation

- PaladinTests passed, including near/far territory, water barriers, rival control preservation, dateline connection, small irregular civic core and tribal revision checks.
- PaladinArtCheck passed in full, including PaladinWorldSurfaceTests and application smoke checks. Two earlier attempts hit the unchanged raw-globe software-renderer 80 ms worst-frame limit; the complete isolated rerun passed without relaxing the limit.
- Additional input regression passed after that full art run: choosing Fortress, pressing Select Region again, then clicking a fortress-only valid site opens naming and creates the 192x192 local map. Globe city founding and flat return coordinates are checked too.
- Animation clock assertions cover 1x/2x/3x/5x and pause. Actual normal/close day/night images, the ruler panel, both territory models and the seven-frame solar-contact sequence were visually inspected.
- Actual Debug executable, Direct3D 11, generated 576x576 city, 28 realms / 85 settlements, 120 frames per case:

| Case | Speed | Mean ms | P95 ms | Worst ms |
|---|---:|---:|---:|---:|
| Continuous zoom | 1x | 5.713 | 17.822 | 226.473 |
| Fast scroll | 1x | 4.200 | 7.238 | 8.570 |
| Continuous zoom | 5x | 5.332 | 30.101 | 51.049 |
| Fast scroll | 5x | 2.731 | 4.716 | 6.784 |

The benchmark includes cold/changing views and runs the real application renderer with the environment clock unpaused. It is not a developed-city simulation-load benchmark; initial asset warm-up still causes a frame spike. No builds ran during profiling.

## Review images

- `tweak-sun-contact-0.png` through `tweak-sun-contact-6.png`: exposed disc through full occlusion.
- `tweak-territory-civic.png`, `tweak-territory-tribal.png`: three nearby connected settlements and a distant isolated settlement.
- `realm-inspection.png`: ruler spectrum meters.
- `realm-city-normal-day.png`, `realm-city-normal-night.png`, `city-motion-day-0.png`, `city-motion-night-0.png`: normal and close city art.

## Executable hashes

- `out/build/x64-Release/Paladin.exe`: `0e7f694ce0cf0ebe72cf088afca1bf2b171e22db42f5434e6e26349a5cea2bbb`
- `out/build/realm-pass-Debug/Paladin.exe`: `ab11cd5abfc201ffbadfba9ceecac0b5bb20da61cb60f68f58322b37c74d0206`
