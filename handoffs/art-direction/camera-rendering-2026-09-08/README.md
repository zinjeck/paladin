# Camera and rendering fixes — 2026-09-08

Updated the local C:/Paladin checkout and both Debug/Release executables. No source sprite PNGs or palette entries were changed.

## Changes
- Polar surface materials use a local spherical mapping, and polar relief stamps use tangent coordinates with latitude-adjusted spacing. Material sampling avoids interpolating incompatible coordinates, which produced streaks.
- World and city detail blends across zoom ranges and briefly blends newly prepared detail into the existing scene. Cold forest and coastline work is budgeted across frames.
- Flat world mode draws one canonical map with black exterior and rejects outside-map picks. Removed the minimap toggle; M switches between flat and globe views.
- City minimum zoom fits the map with a small margin and is enforced during camera clamping as well as zoom input.
- City clouds have stepped pixel silhouettes, are clipped to map bounds, and are separated farther from their shadows.
- Cached static building commands, road/ground textures in shared atlas pages, batched ordered drawing, reusable coast material/contour samples, and bounded ground prewarming remove repeated camera work. Animated doors, roofs and animated accessory recipes retain their live rendering paths.
- Heavy rendering implementations moved out of headers so Debug uses optimized rendering code consistently. Simulation/application runtime debugging remains enabled.

## Verification
PaladinArtCheck passed in Release. Inspected normal/close city coasts, day/night buildings, clouds, flat-map bounds and polar/near relief captures. Debug and Release Paladin.exe rebuilt. Focused dense-forest/building camera profiling used the Windows GPU renderer with simulation inactive.

| Debug camera case | Average ms | Worst ms |
| --- | ---: | ---: |
| Building-area continuous zoom | 5.56 | 17.52 |
| Coast continuous zoom | 5.64 | 13.39 |
| Fast scrolling, day | 3.81 | 7.27 |
| Fast scrolling, night | 3.79 | 7.62 |

The original building zoom spike measured 135 ms, and the original cold forest view measured 172 ms. The final cold forest view at four pixels/tile measured 7.89 ms. Measurements cover the focused fixture, not a guarantee for every save, map size or machine. Detailed content can fade in briefly while its bounded preparation completes; close-up source detail is retained.

Review captures are in review/. Logs are saved beside this note. The pre-existing CMake SDL3_image packaging cleanup is preserved.
