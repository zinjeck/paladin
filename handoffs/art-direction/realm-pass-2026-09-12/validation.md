# Final validation — September 12, 2026

Implemented in `C:/Paladin` on `8c970ea` (merged PR #25). Source edits remain in the working tree. No commit or PR was created.

## Results

- **PaladinTests: PASS**, using the Debug test executable. Includes 128 founding-party seeds, oldest-son/relative succession, archived ancestry, dynasty replacement, fortress dimensions/control, neighboring-realm generation, and 180 years of AI succession with cadence invariance. The synthetic two-continent world contains 12 AI realms, 61 settlements, and two empires. Existing food, labor, construction, demography, geography, and territory checks also pass.
- **PaladinArtCheck: PASS**, using Release and SDL's software renderer. Both `PaladinWorldSurfaceTests` and the full `PaladinApplicationSmokeTests` ran with their default checks enabled. The focused city-review option was not enabled for this run.
- **Native marker motion: PASS**, 480 frames with identical translated plates, including rolled and flat cameras. CSV retained alongside the captures.
- **Coastal political masking: PASS**, zero sea pixels receiving political ink. All four globe/flat and Political/Terrain combinations passed the black-corner check.
- **City motion: PASS**, eight consecutive physical-pixel shifts across source-texel boundaries in both day and night. Day geometry is bit-exact under translation. The existing filtered night light field allows at most 2/255 channel rounding; every frame still has the same exact common art-pixel pitch.
- **Application routing: PASS**, including name cycling, minimap alternation with map-mode preservation, Fortress → region → name → 192×192 map, and rejection of local-map creation/presentation for AI settlements.
- **Visual inspection completed:** founding panel, fortress chooser/toolbar/map, realm inspection, normal and close city day/night scenes, normal/close globe views, coast masks, native markers, and solar-limb captures. Source artwork was not changed.
- **git diff --check: PASS.**

`missing.png`, invalid-recipe, and off-palette rejection messages in `art-check.log` are intentional negative-test fixtures. The final run ends with the successful application-routing message.

## Local builds

Use `C:/Paladin/Play Paladin.cmd` to launch the refreshed Release game.

| Build | Executable | SHA-256 |
|---|---|---|
| Release | `C:/Paladin/out/build/x64-Release/Paladin.exe` | `3E07AC481AEDA9DE2343E52912C7193207CD4F78D1A6BE1BC40FBF0A6A170CCA` |
| Debug | `C:/Paladin/out/build/realm-pass-Debug/Paladin.exe` | `F09402D10098F583147CEF6D8E7585D0EF798968BF549B7FE606CAB332DB0597` |

The already running game and its Debug symbols were locked. The new Debug build uses an isolated directory; the running game was not terminated. Build dependencies were copied into the ignored `.cache/realm-pass-deps/` directory to avoid interference from Visual Studio's automatic dependency regeneration. The project's pinned SDL versions are unchanged.

## Actual Debug camera profile

Ran the actual game executable with `PALADIN_CAMERA_BENCHMARK=1` and `SDL_RENDER_DRIVER=direct3d11`. The final generated scenario had 44 realms and 150 settlements, including a 576×576 player city map. This is a generated terrain/camera scenario, not an exhaustive benchmark of a developed city. World generation uses a new seed for each run.

| Scenario | Frames | Mean | 95th percentile | Worst |
|---|---:|---:|---:|---:|
| Continuous zoom | 120 | 4.01 ms | 7.049 ms | 174.56 ms |
| Fast scrolling | 120 | 2.823 ms | 5.517 ms | 5.809 ms |

Cold frames are included. The zoom run still includes an initialization/cache outlier; these results do not claim that every frame is below the display budget. Raw results are in `debug-camera-profile.log`.

## Review files

Start with `realm-founding.png`, `settlement-choice.png`, `fortress-local-map.png`, `realm-inspection.png`, `realm-city-normal-day.png`, `realm-city-normal-night.png`, `uniform-pixels-day.png`, `uniform-pixels-night.png`, and `realm-pass-sun-limb.png`. The `pr25-*` captures and CSV retain the coastal/native-marker/solar regressions. BMP captures were losslessly converted to PNG.

Implementation choices and tuning are documented locally in `C:/Paladin/docs/realm-foundation-pass.md`. Fortresses currently use the existing finite founding supplies; city resupply and inter-settlement trade remain deferred.
