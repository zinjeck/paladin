# Local graphics and enclosed-building pass — September 8, 2026

Implemented directly in C:/Paladin. Debug and Release Paladin.exe were rebuilt successfully. No PR or commit was created.

## Buildings
- Enclosed definitions reserve a one-tile wall ring. Houses are 5x5 outside and retain a 3x3 room. Bakeries have a minimum 5x5 blueprint; undersized drags are rejected. The city keep is 5x7 outside with a 3x5 interior (its previous interior shortened by two tiles).
- Preview outlines show the outer footprint and usable room. Doors occupy non-corner wall cells. Wall cells block movement; rooms and doorways are walkable. Entry, exit, beds, household wandering and relocation use the interior and doorway geometry.
- Beds retain their dimensions. Furniture is inset into the room. Storage and employment capacity use interior area, keeping walls from creating extra capacity; founding keep storage remains 100.
- Shared adobe wall textures keep their scale across wider facades. Cutaways show textured tile-wide caps, bevel lighting, recessed edges and staggered masonry joints. Shared accessories remain separate.
- Thatched roofs use a pitched mesh with hip faces, a visible ridge and stable uneven straw edges. Variants and dormers fit the expanded outer footprint, with the common final 16-art-pixel tile grid.

## Terrain and weather
- Globe relief receives directional slope shading. Water uses continuous shore-distance depth shading with rounded continental shelves and deeper offshore colors. Shelf distance wraps across the longitude seam and is prepared on the atlas worker.
- A soft concentrated solar reflection follows the sun and view direction, masked to daylight-side water at both globe detail levels. No globe clouds.
- City cloud bodies appear only well zoomed out, at low opacity. Matching drifting shadows remain at every zoom. Motion follows the pause-controlled presentation clock; cloud textures are reused.
- City grass retains detail everywhere with more even coverage, slightly broader connected clusters and fewer isolated contrasting dots. The source palette is preserved.

## Verification
- Debug and Release executables linked successfully: final-debug-build.log and final-release-build.log.
- PaladinArtCheck passed, including actual final pixel blocks, day/night rendering, roof continuity and integration. Cloud checks verify repeatability while paused, motion when time changes, no close-zoom cloud bodies, and persistent close-zoom shadows.
- All four CTest targets passed: final-tests.log. Placement, interior navigation, beds, family behavior, construction, storage and longer simulation scenarios passed.
- Direct3D11 globe review: 3.96 ms mean, 5.55 ms worst during warmed zoom/rotation; one global atlas build.
- Direct3D11 dense-city review at 1920x1200: warmed means 0.61–2.26 ms and worst measured frames below 4 ms through the zoom sequence. Initial cold scene setup was approximately 151 ms; this is not a claim of zero initial loading cost.
- Hardware building gallery passed. Inspected day/night, roofed/cutaway, and multiple zoom levels. See tribal-day.bmp, tribal-interiors.bmp, tribal-night.bmp, globe-noon.bmp, and cloud-body-far.bmp.

The building rules are also recorded in AGENTS.md for subsequent work.
