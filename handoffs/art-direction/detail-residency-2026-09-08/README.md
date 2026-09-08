# Detail residency regression fix

The previous moving detail atlas covered only 128 by 96 longitude/latitude tiles. Replacing it discarded previous coverage and restarted its fade. Near poles it could not cover the visible longitudes, and the fixed source-column margin omitted mountain stamps that extended into a patch. Consequently terrain representations disagreed and coarse terrain persisted at close zoom.

Changed GlobeRenderer to retain fixed 48-by-48 detail regions shared by globe and flat mode. Visible regions are selected from curved sphere bounds, including the pole; detail requests start only in the adjacent close zoom range. Each completed region remains resident while visible. Offscreen regions are retained for returns and evicted by age beyond the 64-region cache target. CPU generation remains asynchronous and texture uploads remain striped. Newly encountered regions blend in once; camera motion does not reset an existing region's fade or replace its coverage.

All regions evaluate the same intersecting relief stamps, including those that cross polar longitudes. Region texture coordinates are clamped against floating-point boundary error. No sprite source or palette changes.

Verification: Release PaladinArtCheck passed. Regression fixture includes a fully mountainous polar cap, verifies full visible detail readiness, checks that panning and zooming out/back do not increase detail-build count, and inspects a settled polar detail capture. Close/normal captures and existing city day/night checks passed. Both Debug and Release Paladin.exe rebuilt locally.

The software-renderer globe sweep averaged 40.08 ms and peaked at 65.02 ms; this is the headless software art check, not GPU game performance. Initially unvisited regions still require asynchronous preparation before their detail appears; resident terrain no longer drops back because the camera moves.
