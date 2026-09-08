Implemented locally in C:/Paladin, 2026-09-08.

- Tree species replace existing tree visuals one-for-one. World-derived temperature smoothly increases conifers from warm regions (zero at .65 and above), through temperate (~11% at .5) and northern (~52% at .4), to exclusively conifers at .34 and below. Tundra/polar always use conifers. Birch is 6% of remaining deciduous sites. Natural feature generation/counts and source sprites are unchanged.
- All interior walls now share the quarter-tile cutaway height. The cap is one connected ring with doorway gaps, outline depth and short floor contact shading. Exterior walls/roofs retain their existing height.
- Confirmed the local removal of the home-footprint collision bypass. Added a regression proving a partially advanced home path into a wall is rejected, plus existing doorway routing checks.
- All adjustable hover previews are one tile. Catalog-wide normalization enforces at least 3x3 outdoor structure zones and 5x5 enclosed structures; roads remain 1x1 and fixed buildings retain their footprints. Placement checks reject undersized footprints.
- Updated older 2x2 test fixtures to legal 3x3 plots. Capacity, livestock-fullness and exclusive reservation tests still exercise their original behavior using the new areas.

Validation: Release and Debug Paladin built successfully. PaladinArtCheck passed. CTest: all four tests passed (unit/gameplay, git-ignore, application smoke, render performance). Hardware gallery passed; inspected daylight, nighttime, close building and cutaway output. Logs and screenshots are in this directory. No Git commit, push or PR created.
