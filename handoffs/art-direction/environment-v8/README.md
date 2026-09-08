# Paladin environment and lumber pass — 2026-09-07

The active project is C:/Paladin (C++/SDL). Source PNGs and the approved 64-color palette were preserved. This pass changes presentation recipes, reuses existing textures for chimneys and logging props, and adds the heating/forestry simulation.

## Presentation

- Fishing Grounds use the existing wooden plank surface, with wooden station props. Their fallback colors are brown too.
- Roof anchors on houses, keeps and bakeries now overlap their front facades, closing the gap caused by transparent image padding. Whole-wall cutaways remain unchanged.
- The building-ground blend fades across the inside and outside of the foundation. It no longer ends abruptly at the rectangular footprint inside the roof overhang.
- Roads share a continuous material contour across connected tiles, with rounded ends, outer corners, inner corners and feathered edges. Each surrounding blend patch has one owner to avoid dark seams from duplicate blending.
- City coastlines use continuous contours, wet bank edges and moving wavelets. Beach/grass and shallow/deep borders follow the same contour treatment. Simulation tiles and placement rules remain unchanged; strategic terrain is unaffected by city shoreline shaping.
- Homes have mud-textured chimneys. Smoke appears only while an occupied home has fuel burning. Smoke, logging tree growth and axe movement use the same pause-aware presentation clock as water, grass and roofs.
- F8 still toggles environment artwork; F6 reloads art. Replacing the existing sprite exports remains supported.

## Home heating

Each home has a physical, lumber-only inventory holding eight logs. It begins empty. Residents can collect firewood even when employed; unemployed haulers and stockpile employees can also replenish occupied homes. Both travel legs are checked before reserving a delivery. Goods already reserved for another delivery cannot be burned.

Homes refill when their stored supply is two logs or fewer. Lighting a fire consumes one delivered log, which lasts twelve game hours, or six in winter. Normal consumption is therefore two logs per occupied home per day, four in winter. Unoccupied homes do not consume fuel or emit smoke. A log already burning continues to provide warmth even if the stored inventory displays zero.

An unheated home applies a 36-point happiness penalty per day, scaled by time without fuel and combined with the citizen's other living conditions. Winter cold removes 18 health points per day while unfueled and suspends healthy recovery for that cold period. Homeless citizens are also exposed to winter cold. Both causes appear by name in the attribute breakdown. House inspection shows fire status and stored fuel.

## Logging Grounds

Choose Logistics → Logging Grounds, then drag out a rectangular area (minimum 3×3). It is an outdoor, walkable workplace with a four-lumber setup cost. Its worker limit scales with area, approximately one worker per nine tiles, and uses the existing staffing controls. The selected area is fixed after placement, as with the other existing outdoor workplace footprints.

Production is `min(attending workers, area / 9) / 30` lumber per game minute. One continuously attending worker with nine tiles produces 24 logs per twelve-hour shift, before breaks, meals, travel and storage limitations. No output accrues with zero attendance or full storage. Output is deposited in workplace storage for physical hauling.

Decorative trees use the existing modular trunk/branch/crown sprites. Workers occupy separate work positions beside them and animate their axes. An attended tree repeats a 24-second presentation cycle: mature for 12 seconds, stump for 3, sapling-to-mature over 9. Each tree has a stable phase offset. These trees are presentation-only: cutting and growth do not consume natural resources, gate production, or award extra lumber. Unattended decorative trees remain mature.

Ordinary harvested trees schedule a return after 12–20 game days, varied by location. Only due stumps are checked, rather than scanning the map each minute. Regrowth is canceled where a completed object or construction site now occupies the tile, including Logging Grounds. Rocks do not regrow. Construction initially clears real resources through the existing gathering jobs; the decorative logging grove then takes over.

## Validation and files

- Core simulation tests: C:/Paladin/out/environment-v8-core.log.
- Build: C:/Paladin/out/environment-v8-build.log.
- Application render/UI checks: C:/Paladin/out/environment-v8-smoke.log.
- Palette audit: active-sprite-verification.csv and verify-active-exports.ps1. All 47 active PNGs have zero off-palette visible pixels and zero partial-alpha pixels.
- Visual captures: previews/home-cold, home-heated, home-smoke-moving, keep-close, logging-mature, logging-stump, logging-regrowing, coast-day, coast-paused and coast-moving, plus the full object gallery.

Tests cover physical fuel delivery by an employed citizen, fuel reservation protection, seasonal consumption, empty-home behavior, cold health/happiness effects, conservation including burned logs, natural-tree regrowth and occupied-tile exclusion, logging attendance/area production and continuous surface joins. Render checks cover fuel-dependent smoke, frozen versus advancing smoke positions, zoom continuity, roof cutaways, day/night lights and the art toggle. Paused coast captures are byte-identical; an advancing capture differs.

The built application is C:/Paladin/out/build/x64-Debug/Paladin.exe. Restart that build to load code changes. Lasting review assets remain here, while out contains builds, logs and disposable test fixtures.
