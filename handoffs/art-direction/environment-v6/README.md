# Paladin: broader pixels, modular trees and themed night

Active project: C:/Paladin. Restart C:/Paladin/out/build/x64-Debug/Paladin.exe for the renderer changes. F8 still toggles environment artwork. F6 reloads exports and presentation settings. O retains the roof cutaway.

## Appearance

The environment now uses broader nearest-sampled pixels. Legacy environment textures are sampled at half their native resolution into cached GPU textures, preserving their original files and palette. This covers buildings, ground materials, props, rocks, resources and the world marker. New roofs and tree parts are authored/exported at the target density already. People, animals and UI retain their previous treatment.

assets/sprites/pixel-density.txt controls this presentation sampling: 2 is the current setting, 1 restores original texture density, and 3 or 4 are coarser. F6 applies a changed setting. It does not alter logical building sizes. New environment-v6 art bypasses this extra sampling because it is already sized for this direction.

The simplified thatch has large bands rather than fine straw scratches. House and bakery share the same 56x50 roof export; the keep uses its 56x114 extension with matching thatch. The house remains mud and reed. Jungle and pasture ground use the quieter rich green grass material instead of the busy pasture pattern.

## Trees and clearance

Three trunks, three branching structures and three crowns combine into 27 possible recipes. Selection uses a stable hash of the map seed and tree location, not the simulation random generator. Trunks stay fixed while branches and crowns sway subtly, with different phases.

Tree presentation checks the building silhouette including roof overhang and the wind envelope. Crowded trees use available room within their existing tile, smaller crowns, and reduced height when necessary. This is visual clearance: it does not remove harvestable resources, change yields or move their occupied map tiles. Trees wholly inside an invalid completed-building footprint cannot be drawn through its walls. Construction and pathfinding rules are unchanged.

The clearance cache is spatially bucketed, updates for building changes, and invalidates when art reloads or toggles. The current crown envelope supports these supplied parts; substantially wider/taller replacement parts require updating that envelope along with their catalog dimensions.

## Shadows and night

Roof shadows now use the source alpha silhouette rather than a large solid L-shaped block. Their cast is shorter, less opaque and softened at the edge. Tree contact shadows follow the displayed tree size. Cutaway walls retain a small contact shadow.

Night ambient light is cool blue-indigo. Local lamps are amber (#FFA04C) and warm gold (#FFB65C), with restrained bloom and independent gentle flicker. Time of day still controls the transition. Nearby buildings still block local light. Illumination remains independent of the 64 authored colors.

## Files and verification

- Source art, prompts and export scripts are here; generation-prompts.md records the built-in image_gen calls.
- Game exports: C:/Paladin/assets/sprites/environment-v6.
- Active recipes: C:/Paladin/assets/sprites/sprites.catalog.
- Original catalog snapshot: catalog-before.txt.
- Day/night, motion, roof cutaway and gallery previews: previews.
- Independent audit: active-sprite-verification.csv; 45 active PNGs have zero off-palette visible pixels and zero partial-alpha pixels.

Debug build and the full software-rendered application smoke checks passed. Added checks cover stable part selection, variation in all three component categories, fixed trunks/moving foliage, and crown clearance around a crowded house while preserving the natural-resource count. Existing fitted-building, door animation, art toggle and terrain checks also pass. Final day, night and environment gallery screenshots were visually reviewed.
