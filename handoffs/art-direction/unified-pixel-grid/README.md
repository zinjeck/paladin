# Unified pixel direction — 2026-09-07

## Required standard

All world art uses one final grid: **16 art pixels per logical tile**, capped at native screen resolution when distant. `WorldPixelScene` covers both city and world scenes, including fitted roofs, resized rocks, grass, entities, attachments, animation and lighting. Nearest-neighbor presentation enlarges that common grid. The target is allocated at window size and reused during zoom, avoiding texture allocation on every zoom step. Interface text remains at screen resolution; goods icons use their native pixels.

Import detail also uses the shared constant. Grass originally stretched a 32-pixel texture across four tiles, giving it only eight pixels per tile. All grass material recipes now cover two tiles, matching the 16-pixel standard rather than relying on final downsampling alone. Larger shapes and patches may still occupy several pixels; the grid does not require every material to have identical texture complexity.

## Art direction

- Clearer, warmer daytime greens and stone highlights use substitutions entirely within `config/art-palette.hex`. Cast shadows use plum rather than near-black gray. Cool night lighting and warm local lights remain separate from the authored palette.
- Official visual reference: https://ngnl.jp/tv/ — luminous daylight and colored contrast, not a gray wash.
- Exterior pots/baskets/wood piles no longer overlay house walls. Stable house variations use shutters, hanging hides, timber sills and lintels mounted within the wall plane. Roof-off interior storage remains interior furniture.
- Atlas export crops now use individually reviewed object bounds. The old equal-cell crop included neighboring foliage in the meat icon. All six exports were recropped, including the fence and planted rocks. Meat, fish and crate cutouts now have a regression check rejecting foliage-colored pixels.
- F6 also reloads HUD resource icons, and those icons no longer stretch from 32 pixels into an arbitrary 28-pixel width.

## Gate and evidence

Build target: `PaladinArtCheck`. It depends on the packaged application and smoke-test executable and fails on a rendering regression. The test reads every final pixel in real-asset day and night scenes at 64 screen pixels per tile and checks every 4×4 display block is identical. Grass source-to-world density is checked independently. Existing animation, pause, occlusion, bed, fence, palette rejection and routing checks remain active. The rule is also recorded in the project's `AGENTS.md`.

Release art gate and core tests passed. Dense 12,160-tree zoom benchmark passed: built-area mean 18.85 ms, worst 30.90 ms; forest mean 12.73 ms, worst 17.72 ms on the software-rendering fixture. These are fixture measurements, not a guarantee for every city.

Review captures live in `previews/`. Runtime source PNGs remain under `assets/sprites`; no user Krita originals were modified. The existing exact-palette audit passed for all 85 referenced PNGs (118,196 pixels, binary alpha).
