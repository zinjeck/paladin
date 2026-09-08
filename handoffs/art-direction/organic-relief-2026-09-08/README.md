# Organic terrain and forest pass — 2026-09-08

Implemented directly in C:/Paladin; no commit, push or PR. Both local Debug and Release executables are rebuilt with matching packaged assets (local-builds.json). The future globe scene remains deferred.

## Changes

- Continuous map-space material coverage creates quiet clearings, denser vegetation and varied shadow coverage on city and world ground. Gentle source-coordinate shifts preserve authored clusters without repeating the same tile-sized rectangle. Terrain biomes retain their own materials.
- World hills and mountains use their local temperature/rainfall ground material. Neighbor-weighted exposure introduces earth and weathered stone, with irregular edges instead of a universal hill color. The overview mountain color matches the warmer detailed material.
- Two hill and two ridge silhouettes, each with six climate vegetation variants, replace fixed repeating relief placement. Stable irregular anchors, varying footprints and overlaps form varied ranges. Coast fragments keep relief out of water. Existing renderer and fallback art remain available.
- Two restrained canopy cutouts identify forest, jungle and taiga. Coverage has clustered density and clearings. Founding a settlement immediately clears a 3x3 neighborhood of canopy around its world marker without changing the biome or invalidating terrain textures. City resources and simulation remain authoritative.
- CPU terrain composition and GPU uploads share a bounded chunk budget. Compiled chunks discard temporary draw lists. Nearest detail fills over the overview as a new area becomes visible; textures are independent of zoom.

## Art provenance

Built-in image generation produced the concepts in source/. The approved ridge is ridge-b-approved.png, its refinement is ridge-refined.png, and ridge-clean.png supplies the extracted runtime cutout. Prompts are recorded in prompts.md. export.py converts selected concepts to 30 binary-alpha PNGs using only approved palette colors. Runtime files are in assets/sprites/organic-relief/; authored source files are unchanged.

The older Godot project was read as reference. Its coherent mountain score combines noise and elevation, and its supported peak threshold follows a lower foothill threshold. The current C++ generator already produces coherent plate-based ridges; the broken visual transition came from the rendering layer's city-grass hill material and repeated relief placement. This pass preserves generation topology and corrects its presentation.

## Verification

- PaladinArtCheck passed, including actual final day/night pixel-block checks and sprite integration.
- Release CTest: 4/4 passed, including rendering performance.
- audit.py: all 30 exports match palette, binary alpha and canonical dimensions. Actual close-view terrain/canopy screenshots have uniform 2x2 output pixel blocks. Before/after founding screenshots differ only near the settlement clearing. City detail coverage varies from 0% to 62% in sampled 64px neighborhoods. Results: audit.json.
- Manually inspected every exported silhouette/variant in previews/all-exports.png, normal and close world/city screenshots, and day/night output. Initial streaking from excessive coordinate warping and oversized gray exposure patches were corrected before the final review.
- Debug and Release builds succeeded; all 30 packaged PNGs and catalogs match source hashes.

## Review files

previews/organic-world-normal.bmp, organic-world-close.bmp, organic-city-normal.bmp, organic-city-close.bmp, organic-forest-before.bmp, and organic-forest-founded.bmp are actual renderer captures. The forest comparison omits the marker to expose the ground clearing clearly. Other captured scenes retain standard game presentation and day/night tests.
