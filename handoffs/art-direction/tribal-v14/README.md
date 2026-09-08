# Tribal households, pasture fences, and resources

Runtime exports: `C:/Paladin/assets/sprites/tribal-v14`. Built-in imagegen produced the source atlas; `tools/export.ps1` extracts its cells, removes connected checkerboard background, preserves enclosed stone highlights, and exports nearest-palette opaque pixels with transparent backgrounds. The authoritative palette is `C:/Paladin/config/art-palette.hex`. No artist-owned originals were changed.

## Behavior

- Co-resident, mutually married adults receive one wide bed per couple. Other residents retain individual beds. Layouts automatically return to single beds on separation, moving out, or death. Navigation keeps separate bed destinations; sleeping presentation places partners side by side inside the shared bed.
- Led animals were rendered directly on top of their handlers. They now travel visibly beside them, with separate depth ordering. Simulation positions and animal reservations are unchanged.
- Pastures have timber perimeter fences, with one-tile access gaps on each side. Sprite mode uses the exported fence along horizontal edges and low timber rails along vertical edges; skeleton mode draws the entire fence procedurally. Perimeter rendering is clipped to the viewport. Far views retain a cheap boundary in the existing object cache.
- Four deterministic rock appearances use two planted rock silhouettes at different proportions, plus stable size variation. Small plants overlap the rock bases. Existing contact shadows remain. Distant rock rendering stays cached and simple.
- Groundpiles show up to nine compact packets, with packet count based on actual goods quantity. Stockpile inventory appears in low open crates, facing the same near-top-down view as the houses. Crates are empty only when the corresponding resource is absent (no decorative fake inventory).
- Goods HUD uses the existing stone and lumber art and the new basket-free fish/meat icons. Icon textures load once. Food/materials keep their existing art where used.
- Unemployment pressure increases from 2 to 2.5 happiness/day. Food-insecurity pressure accumulates 25% faster, with its previous maximum and recovery rate unchanged. Housing pressure is unchanged.

## Verification

Release and Debug builds; complete simulation tests; application rendering/routing smoke tests. Regression coverage includes one and two co-resident couples, restoring four singles, distinct navigation slots, four/three/two rendered bed layouts, fences in both art modes, and visible separation between a led cow and its handler. Exact-palette audit: 85 referenced PNGs, 118,196 pixels, all visible pixels from the approved palette, alpha only 0/255. Visual review exports are in `previews/`.

## Image generation brief

Tool: built-in imagegen (no CLI). Reference: `assets/sprites/environment-v4/nature/rock.png`.

Initial prompt requested a transparent three-column, two-row atlas: horizontal and vertical tribal timber fence segments, loose fish without baskets, a rounded planted boulder, an angular planted rock cluster, and a low open crate. Elevated almost-overhead view, short front faces, coarse 32-pixel-style clusters, palette stone/earth/green colors, no text, ground squares, gradients, or baked shadows.

Final edit prompt: "Edit this sprite atlas. Remove ALL background and colored glow: real transparent alpha outside the pixel silhouettes, no backdrop and no glow whatsoever. Preserve exactly 3 columns x 2 rows and all other sprites. Replace the middle fence top-row sprite with three loose cuts of raw red meat (a steak and two small cuts), no basket or container, matching coarse pixel art. Keep first horizontal fence, third fish, bottom two rocks and crate. Remove steel bands on fence replace with brown rope. Keep all object silhouettes entirely within their cells. Transparent PNG sprite sheet. No text."

The generator still baked in a pale checkerboard; the deterministic export removes only edge-connected neutral background before palette conversion. Runtime assets, not the raw atlas, are the verified exports.
