# City distance rendering and simulation responsiveness — 2026-09-07

Launch the optimized game with `C:/Paladin/Play Paladin.cmd` or `C:/Paladin/out/build/x64-Release/Paladin.exe`. Debug is also rebuilt, but the optimized Release build is intended for playing. Both include the same gameplay and art.

## Changes

- Very distant natural features use one cached density image, with bounded dirty-region refreshes. Forests remain visible.
- At intermediate distances, small static tree crowns and rock silhouettes are drawn into 128px chunk textures directly. This bypasses branch/trunk/canopy composition, shadow generation, sorting, and render-target changes during cache creation. Cache eviction visits at most 32 entries per frame. At most two changed chunks are rebuilt, under a shared 2ms scheduling budget; the previous representation remains visible during preparation.
- Detailed trees fade in between 28 and 44 screen pixels per tile. Their animation is only submitted nearby. Empty feature chunks are not drawn.
- Distant buildings and roads use a cached map-space silhouette with two-tone roofs. Full building details fade in between 16 and 28 pixels per tile. Distant draws are cropped to the occupied area. Selection, placement controls, simulation and inventories are retained.
- Empty construction layers are no longer drawn over the entire screen. Citizen culling now happens before sprite submission; distant people and resource piles use markers. Distant lighting retains ambient color and local warmth without per-building shadow ray calculations.
- The application yields after four simulation ticks or approximately 8ms of tick work per frame (a running tick finishes). Real-time catch-up debt is capped at four ticks, and interpolation is clamped. Under sustained overload the simulation slows rather than trying to recover an ever-growing debt; authoritative simulation steps/events are not skipped. Existing diagnostic counters record discarded real-time debt. Pausing still freezes animation.
- Built an `/O2` Release version, including optimized SDL dependencies, and supplied a simple launcher. No original sprite artwork was changed; distant silhouettes use existing palette colors.

## Validation

Core simulation tests and application rendering/routing tests pass in Release. New checks cover clock debt limits, stable paused animation, a distant tree remaining visible without any full sprite submissions, and harvesting removing that cached tree. Distant benchmark views at 1–16 pixels/tile submit zero detailed scene items in the empty-population fixture.

The same 256×256 software-renderer fixture with 64 homes, 384 roads and 1,664 trees was run before and after. Before is the previously playable Debug build; after includes this patch and the optimized Release build. These are combined improvements, not an isolated LOD-only speedup, and are not claims about hardware-rendered FPS.

| Continuous zoom | Previous Debug mean / worst | Patched Release mean / worst |
| --- | --- | --- |
| Built-up area | 58.71 / 116.03 ms | 16.38 / 33.13 ms |
| Forest | 49.44 / 72.12 ms | 11.26 / 20.49 ms |

The patched Debug build alone did not improve the overall continuous-zoom timing in the initial comparison; optimizing the executable/dependencies and avoiding transparent work were important parts of the result.

A denser 12,160-tree fixture measured 21.02 / 36.84ms over the built-up area and 12.42 / 17.35ms over forest. A separate 128-resident activity/movement scenario ran 240 quarter-minute steps at mean 0.207ms, worst 1.60ms. This is a focused simulation test, not a guarantee for every late-game save. A single unusually expensive simulation tick cannot be preempted by the frame budget.

Reproducible test switches on `PaladinApplicationSmokeTests.exe`: `PALADIN_RENDER_BENCHMARK=1`, optionally `PALADIN_DENSE_FOREST=1`, and `PALADIN_CITY_SIM_BENCHMARK=1` for the activity scenario. Use `SDL_VIDEODRIVER=dummy` and `SDL_RENDER_DRIVER=software` for comparable headless timings. `PALADIN_SMOKE_SCREENSHOTS` selects a screenshot directory. Diagnostic logs are under `C:/Paladin/out/city-speed-*.log`; screenshots remain in this handoff's `previews` folder.
