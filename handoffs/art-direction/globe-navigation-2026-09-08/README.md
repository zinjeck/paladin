# Globe navigation, flat world and minimap — 2026-09-08

Implemented directly in C:/Paladin. No PR or commit created.

## Behavior

- WASD and edge scrolling rotate about screen-relative axes. The globe retains its full orientation through the poles and when upside down, matching the demonstrated RimWorld behavior.
- Dragging uses sphere/arcball rotation: a grabbed surface point follows the cursor. Selection is deferred until release; a drag does not found a city. Losing focus releases the gesture.
- Bottom-right flat minimap marks camera center, shades the surface outside the current view, and shows settlements. Click or drag it to focus a location.
- Flat map / Globe button preserves geographic center and apparent tile scale. Returning to the globe retains roll; panning the flat view updates the focus without discarding the previous globe orientation.
- Flat terrain wraps at longitude, with matching wrapped overlays, sprites and settlement picking. Flat zoom-out is bounded to avoid submitting thousands of copies.
- Stars follow the same planet-to-view rotation. Planet-fixed sunlight and 23.5-degree tilt/season preparation remain intact.

## Shared world representation

WorldGrid remains the canonical data model. WorldSurface converts normalized tile UV to sphere positions and back. WorldMapNavigation handles flat/globe/minimap picking and focus. Camera2D transports an optional globe quaternion, while ordinary flat/city movement clears it.

GlobeRenderer now owns the shared terrain atlas service, used by globe and flat rendering; the minimap reads the same completed texture. Camera moves and projection changes do not rebuild the full atlas. Detailed patches and uploads remain asynchronous/bounded. Old cancelled atlas futures are retired only after completion instead of blocking the render thread.

Runtime terrain changes should use WorldGrid::setTile(position, value). Bulk writes through mutable tile access must end with terrainChanged(). The published revision refreshes the shared atlas once; do not add separate invalidation for each view. Existing generated worlds finish before first rendering. Foliage placement, geographic positions, settlement clearing and daylight tint are now shared by both projections. New SpriteRenderItem content remains defined once in logical tile coordinates.

Source PNGs and palette were not modified. City rendering was not reduced.

## Limited validation

- Debug and Release Paladin.exe rebuilt successfully.
- Focused checks: both poles, full revolution, screen-relative movement while rolled, grabbed-point tracking, minimap mapping, longitude seam, repeated mode toggles, and shared terrain revision invalidation.
- Release Direct3D11 reference run passed: globe warm rotation/zoom mean 3.90 ms, max 5.92 ms, one full atlas build. This is a focused fixture, not a claim about every gameplay scene.
- PaladinArtCheck passed, including actual output pixel consistency and application input routing. Updated an asynchronous cache test to await ready terrain before judging detail.
- Inspected normal/close terrain, day/night globe, flat world and minimap images in this folder.
- Broader gameplay testing is left to the user as requested. No additional CTest sweep or Debug performance sweep was run.

Executables:
C:/Paladin/out/build/x64-Debug/Paladin.exe
C:/Paladin/out/build/x64-Release/Paladin.exe
