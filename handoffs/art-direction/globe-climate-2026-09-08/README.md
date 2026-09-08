# Globe, climate, tundra and city rendering — 8 September 2026

Local C++ checkout: `C:/Paladin`. Debug and Release executables were rebuilt in place. The old Godot project in Documents remains the generation reference. No PR, push or commit was created by this task. The existing user commit is `073f679`.

## Visible changes

- Tundra gains sparse muted moss/lichen using existing palette color `#4F8C7A`. It keeps exposed soil and its cold, inhospitable character. Polar ground remains snowy.
- Landform is now independent of biome. Cold tundra and polar foothills retain hill relief instead of becoming flat when the climate classifier preserves their biome. More varied overlapping hill silhouettes fill slopes while genuine lowland valleys remain open.
- Mountains have distinctly rocky faces, taller pointed cores and irregular snowy caps in dense or sufficiently cold ranges. Hills retain earthy colors.
- Close globe terrain uses a canonical 16-pixel-per-tile patch built asynchronously from the same terrain and relief placement as the global atlas. Fine hills appear only in the close-detail layer. Patches upload incrementally, preserve an existing completed patch during work, and use a border transition. Camera zoom alone does not rebuild the global atlas.
- Climate follows latitude with elevation cooling: cold poles and warm equator. Polar is a separate biome; both tundra and polar reject city founding.
- Continent generation adds varied widths, bends, arms, offshore fragments, bays and channels. The existing Godot-inspired mountain score/support rules remain in use.
- The globe has a readable dark hemisphere, a geographic sun direction that completes one turn per in-game day, and brighter background stars. Lighting follows simulation time, so pausing also pauses its movement. This is an equinox model; seasonal axial-tilt changes are not implemented.

Elevation explained simply: **three land bands, plus water**. Imagine walking from low ground, up rolling hills, to high mountains. Height changes smoothly inside those bands; the world is not three flat stair steps. Snow is a climate/height treatment rather than an extra elevation step.

## City lag and crash causes

The reported frame cost was rendering, not slow simulation. The 1920×1200 Debug forest fixture exposed several causes:

1. Hundreds of thousands of uncached foliage draw commands exceeded a 256-chunk cache. The cache now holds up to 1024 original-resolution chunks (256 MiB), batches the same artwork through an atlas and reuses prepared vertex geometry.
2. Zooming in evicted valid forest caches and discarded their preparation. Valid cached geometry now survives eviction, and GPU textures remain available within the budget. A bounded prefetch pass prepares the next zoom-out footprint and a pan margin.
3. A global construction/navigation revision rebuilt the whole visible forest. Local object fingerprints now restrict clearance invalidation to affected regions.
4. Inland tiles repeatedly examined coast neighbors. Generated coast-region masks skip inland-only regions while preserving the existing coastline and wave rendering.
5. Small cache requests scanned entire neighboring resource chunks before composing tree parts. Early culling includes the full authored overhang and avoids that redundant work.
6. Ground detail was only prepared after zooming closer. The same bounded preparation now runs at wider city zooms, improving detail availability on entry without reducing texture resolution.

The access violation was traced through the active Visual Studio call stack to `Application::renderDebug`: a seven-entry biome-name array was indexed by Hills (and would also fail for the new Polar). The display now calls a checked enum-name function. Regression tests cover Hills, Polar and an invalid enum value. Terrain-name indexing is also bounded.

## Validation and limitations

- `PaladinArtCheck` passed; close/normal, day/night, coast and forest output was inspected.
- Release unit, ignore-policy, application/art and render-performance CTest targets pass. Exact final results are in `final-tests.log`.
- The hardware globe test averaged 2.62 ms, worst 3.44 ms, during zoom and rotation, with one global atlas build (`final-globe-review.log`).
- The original Debug dense-forest test averaged 95.26 ms at 4 display pixels per tile and 48.99 ms at 12. The final panning/zoom measurements are included below. Times measure rendering plus an explicit SDL flush; they are not a promise of total-game FPS for every settlement.
- The test includes a return to the widest view and keeps first-view timings visible. One-time initialization of a brand-new city renderer still incurs synchronous asset/cache work; it is not included in the steady-frame average. This pass does **not** establish a zero-stutter guarantee for initial loading, an arbitrary teleport into entirely unprepared terrain, or every possible city size.
- Generated continent/climate/relief changes require a newly generated world. Restart the executable to load the new rendering code.
- `delivery-audit.json` records executable SHA-256 hashes and verifies all 228 packaged sprite files plus the four configuration catalogs against the source checkout in both build configurations.

Final Debug / Direct3D11 / 1920×1200 dense-forest measurements (milliseconds):

```text
first frame zoom=0.25 ms=1446.12 stages=354.425,0.0151,954.924,6.7799,2.5929,
dense forest zoom=0.25 mean=4.66116 worst=6.3942 cold_worst=1446.12 stages=3.72026,0.00141333,0.378438,0.00451167,0.241125,
first frame zoom=0.5 ms=4.0119 stages=3.3661,0.0017,0.2651,0.008,0.2906,
dense forest zoom=0.5 mean=4.03465 worst=6.0584 cold_worst=5.4174 stages=3.33667,0.00135333,0.191522,0.00468167,0.255048,
first frame zoom=0.75 ms=5.9521 stages=5.0102,0.0022,0.1843,0.0114,0.2941,
dense forest zoom=0.75 mean=5.0012 worst=6.1461 cold_worst=6.2501 stages=4.24231,0.00114333,0.17192,0.006105,0.27675,
first frame zoom=1 ms=5.4149 stages=4.8443,0.0022,0.1706,0.0133,0.309,
dense forest zoom=1 mean=3.91745 worst=8.0488 cold_worst=5.4149 stages=1.8833,0.00106667,1.29028,0.00665833,0.272027,
first frame zoom=1.5 ms=8.6531 stages=0.6501,2.0276,0.1586,3.5653,0.3423,
dense forest zoom=1.5 mean=6.89022 worst=8.9499 cold_worst=8.6531 stages=0.493313,1.62334,0.134243,3.29167,0.287032,
first frame zoom=2 ms=5.3176 stages=0.5126,1.1185,0.1534,2.0358,0.3727,
dense forest zoom=2 mean=4.33164 worst=5.7107 cold_worst=5.3176 stages=0.321812,0.955733,0.121072,1.81986,0.274835,
first frame zoom=0.25 ms=5.8227 stages=3.8152,0.0041,0.7726,0.0148,0.3193,
dense forest zoom=0.25 mean=6.08392 worst=7.6456 cold_worst=6.9482 stages=4.68502,0.00243167,0.5655,0.00875667,0.290123,
```

Review captures: `final-globe/`, `final-city/`, and `final-art/`. Earlier intermediate build and profile logs are retained for traceability; they are not delivery results.
