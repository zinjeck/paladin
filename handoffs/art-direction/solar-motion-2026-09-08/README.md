# Solar time, tilt and city motion — 2026-09-08

Implemented directly in C:/Paladin. Debug and Release Paladin.exe rebuilt. No Git commit, push or PR.

City clocks and schedules now use longitude-based local solar time without changing authoritative world time. Generated settlements retain planetary U/V and latitude; work shifts, breaks and sleep-daylight decisions share that local clock. City lighting samples the same planetary sunlight. The globe projects and picks through a 23.5-degree axial tilt, including the moving star background. PlanetAstronomy supplies hemisphere-aware declination for future seasons, but its live orbital phase remains fixed at equinox. No seasonal progression was added.

Performance findings and changes:
- Prior motion checks mostly exercised warmed slow scrolling. Added rapid sinusoidal scrolling and continuous zoom at 1920x1200 across seven scales in the actual Debug executable.
- City terrain evaluated world-only exposure noise per pixel and repeated tile-wide mountain/interpolation work. Hoisted invariant work and skipped material-transition calculations only where every possible source is identical; output remains identical.
- Initial overview rebuilt/hashes asset names per map tile. Resolve the finite city material combinations once instead.
- Foliage prewarming incorrectly targeted a 4px strategic view even at close city zoom; it now prepares the adjacent zoom range. Visible geometry remains intact.
- Rendering translation units use optimized code in Debug. Application/simulation retain Debug runtime checks and symbols. No source sprites, detail resolutions or close-up features were removed.

Measured Debug dense-forest motion: before means 6.9–12.5 ms, with recurring ~20–29 ms spikes. Final means 2.9–5.7 ms and camera-motion peaks up to 12.1 ms. First-time renderer/assets/cache initialization remains distinct: 668 ms before, 215 ms final. This is not a claim that cold first entry has zero latency. Full application/system stalls outside these measured scenarios are not ruled out. Separate coast review peaked at 20.5 ms.

Validation: PaladinArtCheck passed, all four CTest suites passed, final Debug motion regression passed its new bounds (mean <12 ms, settled peak <20 ms, subsequent zoom-entry peak <20 ms). Tests cover local clocks, midnight wrap, opposite-longitude work behavior, future solstice geometry and tilted off-center picking. Hardware screenshots inspected. tribal-day.bmp, tribal-night.bmp, tribal-interiors.bmp and tribal-zoom-400.bmp were pixel-for-pixel identical to the preceding delivery.

Logs: motion-before.log, motion-after.log, motion-final.log, ctest.log, artcheck.log, debug-delivery-build.log. Screenshots include globe-noon.bmp and the city day/night/interior gallery.
