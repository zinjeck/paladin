# PR29: diplomacy, universal cartography and compact armies

## Baseline and scope

This pass starts from merged PR28, main commit
`48f95546183988af3ac4c7f10eb8a0d5160e9d64`. The verified source archive is
`a63e09f2d0ec4f515e61212dcf39b2c8e527a6f5`, whose only additional file was a
one-time source archive workflow. That helper is removed by this change.

The September 18 request supersedes population-scaled world settlement sprawl
and the large single-column military cards. It does not revert PR28's actual
personnel, employment, movement, treasury or recruitment-origin safeguards.

## Implemented behavior

- World settlements have one universal 18-native-pixel little roofed building
  for cities and one crenelated gate/tower symbol for fortresses. Population,
  realm form and zoom do not change their geometry. The strategic renderer no
  longer draws any settlement sprawl, population stages or their old sprites.
  Real strategic roads are retained. Labels and markers share the existing
  terrain source camera and final residual; strategic entities keep the common
  art lattice. Overlapping armies retain priority over a colliding settlement
  symbol, with the settlement label displaced to remain readable.
- Employment becomes **Diplomacy** only on the world screen. City Employment
  remains unchanged. The panel opens with an empty right side. The left side
  lists every realm, scrolls, and sorts by soldiers, treasury gold, visible land
  area, cities or fortresses. Clicking a sort again reverses its direction.
  Realm selection reveals statistics and these six actions in order:
  **Form Alliance, Send Gift, Demand Tribute / Release Tributary,
  Establish Trade, Declare War, Establish Peace**.
- Outer-zoom clicks use `WorldRealmQuery.h`, the same dry civic/tribal surface
  classification used by thematic maps, selected frontier ink and reported
  area. A tribal region is selected through its displayed influence, not by
  converting it into binary ownership. Clicking opens its diplomacy and lights
  its frontier. Closing the panel clears the selected frontier. Input tests
  exercise civic and tribal selection on both globe and flat projections.
- The bottom-left controls are **P** political, **T** terrain, **G** government
  and **N** population. The strip sits above Back, not on top of it. G uses
  orange for tribal, blue for civic and gray for unclaimed dry land. N uses
  total people per realm, with seven labelled classes: 0, 1-31, 32-127,
  128-511, 512-2047, 2048-8191 and 8192+. These are not tile-density estimates.
  Both new maps have opaque, unshaded thematic land and water, so terrain
  texture, lighting and relief do not leak through. Both projections use the
  same cached page system and classification; population changes refresh it.
- One larger packaged militia PNG represents each nonempty world army.
  Existing male/female walking frames are reused. Only selected soldiers have
  a contour, derived from cached sprite alpha silhouettes, not a box. No
  textures are allocated on a zoom step. Icons, counts, picking and selection
  controls disappear at outer zoom, fade in from 10 to 16 effective pixels per
  tile, and are fully visible above that band. Show on map zooms in enough.
  Sprite and count remain selectable, with stacked-unit cycling preserved.
- Marching now takes **five game minutes per tile instead of thirty**, six
  times faster. The world clock, pause, cardinal routing, seam wrapping,
  mid-leg retargeting and PR28's immediate detachment from city employment
  remain authoritative.
- Military cards are **76x88 portrait buttons in a multicolumn grid at the
  very top**. A normal panel fits seven columns and two rows; overflow supports
  wheel scrolling and a draggable vertical thumb. Filters, reserves,
  recruitment, New unit and roster controls are below the cards. Small
  viewports clamp the panel and expose a safe close-only fallback rather than
  drawing controls beyond its bounds. Press identity survives relayout.

## Diplomacy semantics and explicit boundaries

`DiplomacySystem` changes real live-World relation state. Gifts transfer actual
64-bit treasury money, with positive-amount, funds and overflow preflight;
no gold is created. The default gift is 10 gold and the panel exposes amount
controls. Alliances and trade pacts require peace. War clears those bilateral
pacts and that bilateral tributary link; peace ends the war flag. Tribute
creates a unique-overlord relationship, rejects indirect cycles and switches
the button to Release Tributary. Self-actions and invalid realms are rejected.

This is a deterministic diplomacy foundation: valid offers/demands take effect
immediately. It does **not** invent an AI negotiation/acceptance model,
automatic tribute payments, a caravan/trade simulation, alliance military
intervention or combat resolution. The UI reports the actual stored agreement.
Relations live in the current World; this change does not add a new save-file
format. General cross-city civilian demobilization remains the PR28 limitation.

Statistics count every soldier once. Deployed soldiers belong to their unit's
realm even if their recruitment city was captured. Area means displayed dry
land tiles, not an alteration to territorial ownership. Expensive area scans
are cached by surface/influence revisions; cheap realm statistics refresh by
game minute or panel actions.

## Main responsibilities

- `src/world/Diplomacy.h`, `src/simulation/DiplomacySystem.*`: bilateral and
  subject state, action validation, atomic gold transfer.
- `src/ui/DiplomacyPanel.*`, `RealmStatistics.h`: panel, sorting, identity-safe
  input, real statistics, actions and outcome messages.
- `ApplicationWorldInput`, `ApplicationReports`, `ApplicationWorldScreen`,
  `ApplicationFrame`, `ApplicationSession`, `CityHud`: toolbar context,
  automatic selection/opening, panel lifecycle and input ownership.
- `WorldRealmQuery`, `WorldThematicPalette`, `WorldRealmPresentationRenderer`,
  `WorldRenderer`, `WorldMapNavigation`: shared classification, opaque map
  modes, selection masks, bounded cache preparation and legends.
- `SettlementMarkerRenderer`, `SettlementWorldPresentation`,
  `WorldObjectRenderer`: universal native symbols and removal of sprawl.
- `SceneSpriteLibrary`, `WorldArmyPresentation`, `ApplicationMilitary`,
  `Army.h`: cached silhouettes, matching visibility/picking and marching.
- `MilitaryPanel`: compact top grid and scrollbar.

## Verification

Release application and all test targets built locally with GCC 14.2, Ninja and
repository-pinned SDL dependencies. `git diff --check` passes.

**Mandatory `PaladinArtCheck` passed**, including WorldSurface, application
art/routing and Military UI checks. Coverage includes all four map modes on
both projections, opaque theme colors, population cache updates, both realm
selection types, immutable city/fortress geometry, 150 artwork orientation
comparisons and 480 marker-motion frames, silhouette-only selection, hidden
far armies, compact cards, overflow scrolling, ordered diplomatic actions,
conserved gifts and tribute/release. City normal/close, day/night, fortress,
selected soldier and thematic close-view captures were inspected visually.

**The separate actual application input route passed** with
`PALADIN_SMOKE_UI_ONLY=1`. It covers the real toolbar, all four map buttons,
Back non-overlap, both realm types on both projections, military deployment,
body/count selection, hidden far controls, pause and tile movement.

**The complete normal local CTest run is not all green.** Its independent
software-globe benchmark still exceeds the unchanged 80 ms worst-frame gate;
the final measured mean was 51.6684 ms and worst 93.2202 ms. The threshold was
not relaxed. No controlled baseline comparison establishes whether this timing
difference is pre-existing or introduced. The mandatory art target and
UI-only route do not substitute for that separate gate. Core, asset, gitignore,
military UI, world-surface and separate render-performance tests pass.

New test fixtures were corrected to compare influence revisions around the
render operations under test, rather than against an earlier snapshot that
preceded deliberate terrain/population mutations. Theme colors use the actual
fixture population; the simulator-specific eight-person start is explicit
where required. No source-of-truth ownership mutation is permitted by rendering.

Windows CI retains Debug build/tests, Release `PaladinArtCheck`, separate
application input/deployment and mandatory evidence checks. The live PR records
its final head, run outcome and any remaining limitations; this document is
not a claim that a pending remote run has passed.

## Review evidence

New captures include `pr29-diplomacy-blank.png`, `pr29-diplomacy-actions.png`,
`pr29-diplomacy-tributary.png`, `pr29-compact-unit-grid.png`,
`pr29-military-overflow-top.png`, `pr29-military-overflow-bottom.png`,
`pr29-army-selected.png`, `pr29-army-unselected.png`, `pr29-army-far.png`,
`pr29-fortress-icon.png`, `pr29-fortress-population-invariant.png`,
`pr29-{flat,globe}-{government,population}*.png` and actual application
`pr29-select-{tribal,civic}-{globe,flat}.bmp` scenes. Existing day/night,
walking/gathering/sleep, coast, border, motion and solar evidence is retained.

No new soldier image download or manual asset registration is required.
