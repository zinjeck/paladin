# PR 26: UI, military and food-chain continuation

Continues main checkpoint `4bed07b84a69fc474abac0c67f43c10dad189717`, including the interrupted resource/building/industry definitions. This is the C++ implementation, not the archived Godot game. Stockpiles remain freely drawn, terminology remains Realms, and existing founding and political-surface behavior is preserved.

## Playing the new systems

Build completed barracks and an army supply depot from Military. Barracks jobs employ real adult citizens. Open the city Military panel, use Hire to employ reserves, New unit to create an initially empty world unit, and +1/+5 or -1/-5 to transfer actual enlisted people into/out of it. Dismiss releases unassigned reserves from barracks employment. It cannot silently dismiss deployed/assigned soldiers. World units and In this city are separate lists. Select a row, then Show on map; left-click your army symbol to select it and right-click passable land to march. Escape clears world selection. Rosters can be reorganized or disbanded at their home city; an empty, exhausted unit can be removed remotely. There is no independently editable population counter.

Soldiers are a distinct StrongId-backed entity, linked by (home settlement, citizen ID) to one living person's employment, family, savings and wages. World rosters own soldier IDs, not cloned citizens. Departing soldiers leave the local task, food, family and render loops; returning soldiers resume local activity. Death, workplace removal and ownership changes remove invalid references. Unit movement supports the map seam, rejects impassable routes and preserves position on mid-step retargeting. Combat, naval transport and formation tactics are not introduced by this pass.

## Goods and money

Bakeries purchase wheat and consume one wheat to produce one bread per 30 worker-minutes. Depots buy ordinary edible food and convert one item into one ration per 30 worker-minutes. Barracks purchase rations only from depot inventories. Purchases transfer existing money and goods atomically through the commerce ledger, not zero-price workplace transfers. Inputs are retained for their industries, but unrelated goods in those workplaces remain saleable. Inputs do not multiply storage limits, consume reserved deliveries, or fill all output capacity. Inactive cities use these same recipes rather than freezing a prior output rate.

Units carry at most six rations per soldier from actual barracks stocks. They consume those stocks while deployed, with hunger and health consequences after supplies run out. Friendly barracks can resupply passing units. Roster reductions return excess supplies to barracks or a haulable pile instead of deleting them. Ingredient and military consumption are recorded separately from civilian meals.

Civilian rations are last-resort food. Selection, actual eating and offscreen consumption all check ordinary food in city inventories, reserved food and picked-up hauling claims first. Reserved or temporarily carried food does not incorrectly make rations a normal meal. Wheat is not directly edible. Gather designates wild wheat clumps as well as the pre-existing gatherable animal resources. Farm construction consumes wheat seeds; wheat farm output follows a three-day growing cycle and real harvesting work, not an unlimited hourly source.

## Rendering and authoring

All new source PNG colors belong to `config/art-palette.hex`. The runtime catalog imports 72 four-pose character strips, a sparse grain-stalk sprite, and settlement/town/city/fortress sprites through the existing palpak compiler. There is no second loader and no noise-generated animation. Walking advances from traveled distance; work poses and tool impacts follow the simulation work clock; pausing freezes both. Sleep indicators are whole-pixel 3x5 glyphs with shadows on the city art grid. Soldiers use military equipment sprites and distinct render identities.

Strategic settlement art is integrated in the existing 32-art-pixel world-object raster, sharing its camera/residual, depth sorting and visibility. Native-screen names and symbols are preserved. Current visual stage thresholds are population 128 for town and 1,024 for city; fortresses have their own art. These are display thresholds, not new settlement types or economy rules.

Panel chrome uses the shared native-screen frame with charcoal/blue-gray interiors, ivory labels, steel edges and restrained brass corner fittings. Geometry stays fixed across hover/pressed/selected states. Compact world labels retain the existing tiny bitmap; native-screen body labels use the new hand-authored proportional optical-size serif glyphs. No third-party font files are included. The result follows the supplied concept direction, not a claim that every concept illustration was converted pixel-for-pixel.

`generate_art.py` preserves existing approved character silhouettes and deterministically exports the new strips and strategic art. `serif-glyph-source.py` exports `src/ui/UiSerifGlyphs.h`. Both resolve the repository from their own location. The PNG authoring script requires Pillow; runtime/build users do not need Python to regenerate assets.

## Verification and evidence

The continuation was built locally with GCC/Ninja Release and SDL dummy/software rendering. Focused tests cover money/goods conservation, workplace export restrictions, reserved/carrying food, full-store conversion, wheat gathering/seed costs/growth, inactive recipes, unique enlistment, roster limits, authorization, supplies, travel/retargeting, pause, death and empty-unit cleanup. Render tests exercise real compiled assets, military-panel mouse controls, viewport fitting, walking/gathering poses, sleep glyphs, pixel blocks, pause stability, settlement stages and marching sprites.

Run `ctest --test-dir <build> --output-on-failure` and build target `PaladinArtCheck` with `PALADIN_ART_REVIEW=1` and `PALADIN_SMOKE_SCREENSHOTS=<directory>`. CTest now has seven suites when application smoke tests are enabled. A full local seven-suite run passed; subsequent runs also exposed a host-sensitive existing software-globe worst-frame check (80 ms bound, observed 80.17 and 101.65 ms despite means near 49 ms). That timing bound was not weakened or removed. Inspect CI for the exact published head before merging; local Release evidence does not establish Windows Debug performance.

Actual software-rendered evidence includes pr26-military-panel, ui-states, walk-0..3, gather-0..3, sleep-close/normal, citizen/soldier-close, deployed-hidden, world-settlement/town/city, and world-marching/paused PNGs. Existing day/night, zoom, coast, cartography, sun and 480-frame marker-motion checks remain. The workflow requires the focused PR26 images as well as the existing world regression evidence. Art evaluation remains subject to the user's visual review.
