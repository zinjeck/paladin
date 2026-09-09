# Organic foundations and roads — 2026-09-09

## Objective

Make settlement buildings feel planted into the terrain and make dirt roads read as worn, irregular surfaces without changing any simulation geometry, placement, collision, navigation, road speed, construction rules or building footprints.

## Presentation rules

- The logical tile grid remains exact; only rendering hides the ruler-straight geometry.
- Enclosed-building dirt skirts stay compact so a road can meet a house almost immediately instead of creating a fake front yard.
- Close-view foundation accents are deterministic and snapped to the canonical 16-art-pixels-per-tile grid.
- Foundation stones, clay/timber fragments, contact shadow and sparse weeds may extend a few art pixels onto an immediately adjacent road. This overlap is visual only.
- Door travel strips remain clear of the hard accents.
- Road shoulders use continuous settlement-space variation shared across tiles and cache boundaries. The readable road core is explicitly protected from noise-created holes.
- No source PNG, palette, building footprint or gameplay data changed in this pass.

## Implementation

- `NaturalSurfaceShape.h`: adds stable multi-frequency road shoulder variation plus a protected full-opacity road core.
- `SettlementEnvironmentDetails.cpp`: routes road alpha through the shared road-surface function and tightens the cached building dirt skirt to roughly a few art pixels outside the footprint.
- `SettlementGroundCache.h`: adds sparse deterministic close-view foundation accents above the road layer while preserving the existing cached broad road/foundation surfaces.
- `.github/workflows/pr-ci.yml`: adds a dedicated Windows software-rendered `PaladinArtCheck` job for every PR to `main`.

## Performance constraints

The existing road/foundation texture cache remains authoritative. Broad contours are still cached and locally invalidated. New hard accents are close-view-only (`AnimationDetailPixels`) and bounded by building perimeter rather than area. No per-frame textures are allocated.

## Required verification

- Normal Windows build/tests and asset tests.
- Dedicated software-rendered `PaladinArtCheck` in GitHub PR CI.
- Inspect a house directly against a road, straight road, corner, T intersection, cross intersection and a dense settlement at close/normal zoom.
- Confirm roads remain visually continuous and doorway approaches stay unobstructed.
- Confirm no changes to navigation/placement/simulation behavior.
