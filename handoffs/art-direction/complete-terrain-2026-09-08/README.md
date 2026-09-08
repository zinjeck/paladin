# Complete world detail and city transition stability

## World
The previous implementation started region jobs on zoom and presented them individually. Retaining regions prevented repeated loss but did not make initial transitions coherent. The renderer now prepares the entire world at the canonical 16 art pixels per tile, off-thread, when the world is opened or its terrain changes. The world-loading screen stays responsive and shows preparation progress. Interaction begins only after the base maps and every detail texture are fully uploaded.

Camera movement now only selects resident regions. There are no camera-triggered jobs, no region eviction and no region arrival fades. A single zoom factor controls the whole detailed representation in both projections. CPU detail storage is released after upload. The high-resolution water duplicate is omitted; the existing whole-world water mask handles distant reflection. Polar hill artwork is mapped to the approved snow palette instead of covering polar snow with brown tundra hills.

This deliberately trades initial world preparation and resident memory for complete, immediate camera access. A default 600-by-440 map uses approximately 258 MiB for the high-resolution RGBA terrain, plus the existing lower-resolution maps and other assets. GPU textures are split into bounded regions; the entire source image need not fit in a single GPU texture.

## City
Construction and task updates were restarting a density-overlay fade, producing dark rectangular chunks. Some fallback drawing paths also ignored zoom opacity, so replacing them with cached drawing changed brightness inside LOD transitions.

Removed cache-age fades from natural scenery and terrain. Fallback and cached representations now use the same zoom opacity, including non-batched foliage and terrain fills. Dirty foliage textures are actually replaced when the upload budget becomes available; the previous valid texture remains usable in the meantime. Zoom-based detail blending remains in place.

## Verification
The rendering regression checks cover construction updates at 4.5, 9, 12, 20, 24, 36, 40 and 44 pixels per tile, comparing the updated rendering against the same fixed scene after 220 ms. This checks the overview, terrain, object and close-foliage transition ranges for time-dependent flicker. The whole-world check verifies resident polar detail and no additional detail builds during camera movement or zoom-out/return. Application routing waits for world preparation before interacting.

Final build/check results are recorded in the accompanying logs. Source sprite PNGs and palette entries were not edited.

Final result: Release PaladinArtCheck passed, including the eight city transition checks. Debug and Release Paladin.exe rebuilt successfully. Reviewed polar detail and city day/night captures.
