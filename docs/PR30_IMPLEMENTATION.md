# PR30 implementation and continuity handoff

Date: 2026-09-19. Base: merged PR29 (`2951af4d`). Branch: `feat/pr30-citizenship-logistics-selection`.

## Scope and status

This pass implements the September 18 `PR 30 tasks.txt` and its four screenshots. Stockpile concept art is paused; the stockpile interior, roof and drawable footprint were not redesigned. This document records implemented behavior separately from future warfare systems. The PR must remain unmerged until the user reviews it.

## Delivered task matrix

| Request | Implementation and ownership |
| --- | --- |
| Complete city highlights through zoom | `SelectionOutline.h` snaps shared edges and uses an integral raster-cell stroke. Buttons, trees, buildings and footprint tools use complete contours. `SceneSpriteLibrary::renderSelection` derives a cached outline from the selected animation frame's actual alpha; `CityRenderer` uses the same sorted draw item and citizen/soldier identity as the figure. Selected citizens retain an authored sprite at distant city scales. |
| Detached barracks carpet | `SettlementObjectPlacementController::hasDrawablePreview` separates sufficient footprint size from placement validity. `CityRenderer` does not expand the initial one-tile adjustable preview into a full building recipe. A valid sized drag still previews the structure; fixed rotated keeps still render. |
| Laws and universal reforms | `RealmLaws`, `EmploymentTechnology.cpp`, and `Simulation::changeWorkDay` implement the exact three categories and requested ordering. Defaults are Absolute Monarchy / Blood / Male-Dominated. Alternatives are visibly locked and have no mutation targets. One Reforms pane changes work-day policy for every owned settlement and future local maps. |
| Four technology trees | `EmploymentPanel` has Military / Administration / Commerce / Production tabs, per-tab camera state, wheel zoom about the cursor, clipped branching boxes and drag panning. Empty boxes are disabled placeholders. Administration/Citizenship is the first and only implemented technology. |
| Citizenship and cultures | `CitizenshipSystem`, `SettlementCitizenState`, and `World` maintain primary culture, optional secondary culture, citizenship realm and birth settlement. Research grants current personnel local citizenship/culture and preserves immigrants' old culture. Later immigrants are naturalized once the technology exists. Nearby AI origins carry their own realm cultures. Shared-local-culture parents produce a child with that culture alone. |
| World army interaction | The old selected-army text box covering the top-left HUD is removed. Escape, blank-map left click and repeated single-unit selection can deselect. Existing overlapping-unit cycling, close-only visibility, count picking, silhouette outline and tile movement remain. |
| Disband returns civilians | `MilitarySystem::disbandUnit` preflights the entire roster, returns the SAME canonical people to their recruitment cities, clears military employment/state, returns carried rations and updates civil population. `SettlementPopulation::transferResidents` preserves fractional demographic carry. Disband is not reserve rehiring. |
| Map mode controls and population view | P/T/G/N and their legends are directly above the bottom-right minimap. `WorldPopulationField` replaces uniform realm totals with localized geographic census estimates. Both projections retain terrain, unclaimed land/water, relief and actual road geometry beneath population ink. Government mode still blanks terrain. |
| Movable world panels | `PanelDrag` is shared by Employment/report, Military, Diplomacy, Ledger and settlement inspection panels. Header-only capture, release, focus loss, resize clamping, text entry and wheel routing do not move the world camera or activate underlying controls. |
| City inspection and shipments | `WorldSettlementPanel` opens from a city marker, showing owner, civilian population, present garrison and nine resource totals. Owned cities have resource Send controls, numeric quantities, All, One trip/Sustained, Cancel and owned destination list/map picking. Foreign cities are read-only and do not change the owned active city or materialize a detailed AI map. |
| Actual caravans and overflow | `WorldShipmentSystem`, `WorldLandNavigation`, `WorldShipment` and `WorldObjectRenderer` load real unreserved public inventory, move one covered wagon tile by tile, deliver into storage or near-keep ground piles and return empty. Repeated routes wait for the next full load. Stopping/capture/blockage retains in-flight cargo. |
| AI armies and logistics | `AiRealmSystem` periodically creates law-eligible strategic garrisons using real people and existing food/rations, scaled by population, fortress status and ruler militarism. It dispatches the same conserved food caravans between its own cities and protects civilian food floors. |
| Diplomacy intelligence | `DiplomacySystem` provides pairwise directional opinions, size-aware gifts and geographic range checks. Nearby AI-AI realms periodically decide on trade, alliances and treasury-funded gifts. Actions outside range are disabled and rejected in the simulation. No phantom money is minted. |

## Controls and rules

**Settlement shipments.** Open a city marker, choose Send beside a resource, click/type the quantity or use +/-1, +/-10 or All. Choose One trip or Sustained. Select an owned destination from the list or directly on the map. Clicking the same resource again, Cancel, Escape or right click cancels the unfinished setup. Stop disables future departures but lets an existing loaded trip finish and return. Routes without enough goods wait; they do not pull reserved construction/haul goods or private house/market stock.

**Technology.** Open Technology, choose Administration and click Citizenship. The research is immediate and free in this initial pass; there is no research-time/cost economy yet. Drag empty canvas to pan, scroll to zoom, and drag the title bar to move the whole window. Other boxes intentionally have no effects.

**Culture.** Nearby origins are the nearest at most eight populated AI settlements within 30 angular degrees, independent of camera projection and across the longitude seam. No nearby origin means admission is unavailable, including in the population UI. Origin selection assigns provenance and culture; this is not a physical migration caravan or a deduction from the AI source population. After naturalization, local culture is primary and the previous primary is secondary. The two-culture limit does not retain an unlimited ancestral list. A local-born child with a citizen parent meets Blood citizenship; the common dominant culture overrides dual inheritance when both parents share it.

**Population map.** The legend is estimated people per world tile, with thresholds 1 / 4 / 16 / 64 / 256 / 1024. Detailed-city residents use home/keep locations where available, otherwise a known citizen position. Aggregate AI cities use a deterministic connected dry-land footprint, capped by their strategic city/fortress zone and weighted toward the centre. This preserves their census total but is explicitly an estimate, not a claim that individual AI houses or streets were simulated. Universal city/fortress icons remain; old decorative sprawl is not restored.

**Caravans.** Land-only cardinal navigation, horizontal wrapping and no polar wrapping; ten game minutes per tile. At most 64 active routes per realm, one resource and 1..1,000,000 goods per route. Failed creation does not debit goods. Finished route records may be pruned, active cargo may not. Arrivals in an existing detailed local map fill public stockpiles/keep first and overflow onto walkable ground near the keep. A settlement not yet materialized uses its aggregate inventory. There is no sea transport, escort, caravan combat or cross-realm merchandise exchange in this pass.

**Disband.** Immediate administrative return to each person's recorded recruitment city, not a return-march animation or an automatic barracks reassignment. Population rises because the deployed people resume civilian residence. Missing canonical records or an unavailable return location block partial discharge. Military +/- roster changes remain the separate reserve assignment workflow.

**AI.** Decision work is staggered and bounded, not a full-world per-frame search. Garrison/logistics decisions run daily with staggered realm phases, and diplomacy runs weekly. Military size also respects food and actual eligible personnel, so a ruler does not conjure an unlimited army just because militarism is high. At most eight guards are recruited in a single city decision. AI food routes preserve source subsistence and stop excessive deliveries. Diplomatic range is 45 angular degrees between the nearest settlements of two realms. Suggested gifts use both populations, capped at 5% of the sender's real treasury; an empty treasury cannot fund a gift.

## Explicit future work, not claims of this PR

Combat resolution, battles, sieges, army casualties, conquest targeting, AI invasion campaigns, alliance military intervention, periodic tribute collection and cross-realm trade cargo are NOT implemented. This pass prepares real armies, logistics, relationships and opinions for those systems. Valid player diplomacy still uses the existing immediate state-change model; negotiated AI acceptance of a player proposal is future work. Governors/nobles/commanders have a shared law eligibility predicate but no new appointment UI here. Non-Citizenship technologies and non-default law unlocks remain placeholders.

## Tests and evidence

- `Pr30SocietyTests`: defaults, gender eligibility, nearby origins, primary/secondary cultures, research idempotence, inheritance and geographic wrapping.
- `Pr30ShipmentTests`: physical inventory conservation, one-off/repeated deliveries, reservations, overflow, invalid ownership/destination, stop, capture, blockage, longitude seam, pause and elapsed-time partitioning.
- `Pr30StrategyTests`: census mass and locality, water/seam behavior, real AI person/ration/civil population transfers, preserved demographic carry, personality/sex eligibility, daily/weekly cadence and diplomacy/gold conservation.
- `Pr30RenderTests`: 4,000 outline phase/size combinations, bottom-right minimap positions, drag capture and 144 animated male/female citizen/militia/logger alpha-contour comparisons.
- `ApplicationSmokeTests`: real mouse/keyboard routing for own/foreign city inspection, quantities, one-off/sustained map dispatch, cancellation/Stop, movable panels, tree pan/zoom/research, no-origin immigration, barracks one-tile vs sized previews and rotated fixed keeps. Existing army/diplomacy input checks remain.
- Existing mandatory world art checks remain: 150 orientation comparisons, 480 native marker-motion frames, coast/surface identity, flat/globe modes, smooth native sun and day/night city rendering.
- `.github/workflows/pr-ci.yml` requires the new shipment, wagon, laws, technology, citizen-contour and barracks screenshots in addition to the prior mandatory evidence. Missing artifacts fail CI.

The full local suite includes the unchanged 80 ms software-globe worst-frame limit. Conservative off-screen globe-polygon rejection reduces SDL software triangle work without lowering source detail, changing visible geometry or disabling that check. One scene-lattice cell of guard coverage is retained. The PR description records the final verified commit, local result and Windows run; do not substitute an older transport commit's status for final-head validation.

## Superseded assumptions and next step

PR29's opaque population mode and total-people-per-realm classes are superseded only for Population; Government remains terrain-blanked. Top-left selected-army instruction HUD is removed. City-specific work-day reforms are no longer exposed; the Reforms pane is realm-universal. A deployed soldier discharged through Disband becomes a civilian, not a silently rehired reserve. Nearby cultural origins are mandatory rather than an optional fallback.

The immediate next step after this PR is user review of the new city/world interaction and map presentation. Before a warfare implementation pass, agree on battle/contact rules, army damage and retreat, settlement capture, garrison defense and how diplomatic commitments trigger intervention. Do not infer those gameplay rules from the presence of the preparatory military/diplomatic data.
