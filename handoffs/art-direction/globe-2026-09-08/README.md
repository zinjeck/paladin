# Local globe and terrain pass — September 8, 2026

Built directly in C:/Paladin. Both out/build/x64-Release/Paladin.exe and out/build/x64-Debug/Paladin.exe are updated. No commit or PR was created. local-builds.json records executable hashes and verifies the 30 relief assets plus catalog match both packaged builds.

## Behavior

- Left-drag rotates the sphere; the wheel zooms. Picking uses the inverse sphere transform and ignores the star background. Dragging does not accidentally select or found a city.
- Existing generated terrain is displayed on a sphere. This first iteration retains the underlying tile simulation; it does not introduce a new spherical simulation topology or political system.
- Political cartography is now explicitly requested rather than rebuilt implicitly when crossing zoom thresholds.
- World surface atlases are built asynchronously and uploaded in bounded strips. Zoom reuses the atlas. One continuous surface avoids seams between detailed local patches and the overview. Nearby foliage is a separate layer, omitted at regional scale and around settlement markers; textured ground remains underneath.
- City terrain overview generation also runs asynchronously. Chunk work is bounded, and cache textures do not change resolution on every zoom step.
- Coastline shaping previously shared a distance gate with animated waves. Static shaping now remains active at intermediate zoom, while tiny animated details can disappear. Far views use a contoured overview.
- Grass uses continuous landscape coordinates and gently warped material sampling. Density no longer falls to zero, so variation changes the arrangement of authored details rather than producing blank green areas.

## Mountains and elevation

Translated the reference Godot project's mountain score and neighborhood-support rule from Documents/Paladin/scripts/world/generation/WorldGenerator.gd. Peaks require a sufficiently strong score and surrounding upland support. This replaces the previous plate ridge scoring while preserving water topology repair.

There are **three land bands, plus water**: low ground, hills, and mountains. Think of walking uphill: the ground gradually rises into hills, then tall mountains. These are names for portions of a continuous slope, not three flat stair steps. Range centers use taller pointed silhouettes; lower ridges blend outward into hills. No extra discrete elevation layers were necessary.

The regression fixture contains 23,013 land cells, including 6,312 hills and 4,976 mountains. All mountain cells satisfy the support rule. Determinism and band classification are checked against the reference scoring contract across multiple seeds.

## Verification

- Release PaladinArtCheck passed, including actual day/night pixel-grid checks, cutout and sprite integration checks.
- All four CTest suites passed: unit, Git ignore, application smoke, and rendering performance.
- Debug executable rebuilt successfully.
- Inspected day/night, near and far city grass/coasts, whole globe, regional globe, and close globe captures.
- audit.py checked 180 grass patches: every patch retained visible texture, with 18.55–26.27% non-dominant pixels. All 648 sampled 32-pixel regions were distinct.
- Final Direct3D 11 globe warm zoom/rotation rendering: mean 0.681 ms, worst 1.039 ms, one atlas build across the sweep.
- City coast zoom rendering: mean 4.255 ms, worst 16.916 ms. These are measured rendering workloads on this PC, not a guarantee of zero dropped frames under every simulation load.

See gpu/run.log, tests.log, build.log, debug-build.log, and the BMP captures. The globe uses bounded atlas resolution and nearest sampling; very close views expose that finite terrain resolution. It is an initial globe implementation, with separate nearby foliage, not unlimited Google Earth surface detail.
