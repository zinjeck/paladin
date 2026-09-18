# World billboards, city military controls and deployed armies

Base reviewed: `24b6d385bc51e880070f23f70ee9e2121bcbeba9` (merged UI/gameplay work).

## Visible changes

Settlement and army artwork uses an upright 32-art-pixel world-object raster.
Only the ground position follows the globe rotation. Roads still use the rotated
terrain source chart. Both paths retain the shared source camera and final rigid
residual; no per-object coarse snapping, extra source assets or zoom-dependent
texture allocations are introduced. Empty ground/object layers are skipped.
This removes the old `.999` local-weight rotation switch for sprites. The small
native map labels and symbols remain separate from world geometry. A city label
moves above an overlapping army instead of obscuring its head; its small symbol
is suppressed only while a soldier occupies that annotation location.

Political border strength remains 1 at close zoom. The close fill remains .18.
Terrain map mode remains free of political ink; ownership and shore masking are
unchanged. Selected buttons and choice cards receive an explicit four-sided
inside-bounds outline after their skin/label-area rendering.

Rule now exposes Barracks and Army Supply Depot for cities and fortresses.
These use the existing construction, employment, artwork and paid ration chain;
a completed keep is required before placing them. Military uses a scrollable
single Y-axis column of tall cards. New unit requires a real unassigned barracks
employee and assigns one immediately. More employees can be added with +1/+5.

Each nonempty army has one representative soldier, a live count plaque and a
shared renderer/picker footprint. The strategic minimum sprite canvas is 24
screen pixels high; close art continues to scale on the 32-art-pixel lattice.
Walking uses the existing militia frames, and pause freezes the pose. Both the
body and count can be clicked. Repeated clicks cycle stacked units. Right-click
land orders cardinal tile movement, including the wrapped longitude seam.

## City presence versus identity

An army has a current `stationedSettlementId`, not a permanent home city.
A valid movement order immediately clears its station and removes it from the
local unit list. Its soldiers finish local tasks, release barracks employment
slots and stop appearing/working/eating in the city. They cannot be rehired as
unemployed civilians while deployed. Empty/no-op/invalid orders do not detach a
stationary unit. Stopping at any friendly prepared city allows local recruitment
and resupply. Destroying/capturing the origin barracks does not remove field
soldiers or change their army's allegiance.

The existing one-person source key remains for family, savings, appearance and
casualty accounting; this is provenance, not an army movement or employment
requirement. Field wages come from the army owner's treasury and never a
captured hometown's treasury. Actual rations are loaded/consumed, not generated
by creating or moving a unit. Local and field needs do not both consume food.

General civilian migration is outside this change: demobilizing an individual
still requires that person's recruitment city with a free barracks seat. A
mixed-origin army can recruit in multiple cities and move independently, but it
cannot teleport people into another city's civilian records. Disband checks the
entire eligible roster first and refuses partial or duplicating demobilization.

## Regression coverage

- MilitaryIndustryTests: employment-backed reserves, one-soldier creation,
  no-op versus immediate departure, authority, half-tile interpolation,
  pause, casualties, exact-person return, lost/captured source barracks,
  friendly-city reinforcement, field cash conservation and longitude wrap.
- MilitaryUiArtTests: real panel input and portrait geometry; one versus two
  soldiers changes only the count plaque, not the representative character;
  full body/count hit regions from far through close zoom; four-sided outlines
  with procedural and nine-slice skins; both Rule menus; 150 centered renders
  across five geographic regions, six scales and five rolls, plus the former
  local `.999` transition. Existing walk/gather/sleep/pause checks remain.
- WorldSurfaceRenderingChecks: border-only ink with zero fill, no sea ink,
  unchanged .18 fill, plus existing 480 marker-motion and coast checks.
- ApplicationSmokeTests: actual application events create/select/cycle units,
  pick their heads and count plates on flat/rolled globe views, block orders
  through the status panel, march half a tile, respect pause, and remove moving
  units from the city tab. The existing asynchronous terrain input gate is
  awaited before the fortress placement regression.

`PaladinArtCheck` remains mandatory. CI additionally runs the full UI/event
routing scenario separately from the art-only capture mode and requires the
new screenshots in the uploaded software-art review artifact. No interactive
Windows/GPU play session is claimed by these software-rendered tests.
