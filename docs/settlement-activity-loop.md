# Settlement activity loop â€” September 5, 2026

This pass replaces the former attendance controller with a single settlement-owned activity authority. Employment records relationships; needs create urgency; the activity system alone selects and executes tasks. Movement executes routes and supplies bounded idle wandering when there is no activity.

## Current gameplay rules

- Founding the keep grants exactly 40 lumber, 40 stone, and 20 fish once. Keep capacity is 100 total units. Stockpiles hold 250; other workplaces currently hold 50. Reservations share that total capacity across resources.
- Employed workers default to 06:00 through 18:00 (12 hours). Laws provides city and realm workday arrows, in one-hour steps from 0 to 24, always centered on noon (nine hours is 07:30-16:30). A realm enactment updates all controlled city maps and the default for future maps; a city enactment adjusts only that city. Unemployed citizens answer commands, construct, and haul at all hours, returning home when no work is available at night. Food seeking continues at all hours. An already picked-up load can finish delivery after the shift.
- Hunger starts at zero and rises by 50 per 1,440 game minutes. At 50, a citizen seeks one physically accessible food unit, which removes 50 hunger. Carried edible cargo may supply a meal, reducing both cargo and its delivery reservation by one.
- Above 75 hunger, starvation damage rises linearly with severity. A healthy citizen loses 25 health by 87.5 hunger and reaches zero health at 100 hunger without food. Existing injuries can cause earlier death. Below 50 hunger, health recovers at 25 points/day.
- Happiness pressure comes from hunger, poor health, and accumulated homelessness. Homelessness begins gently and grows over days; housed, fed citizens can recover happiness. Completed houses automatically house four residents each.
- Trees and natural rocks yield four lumber and four stone respectively. Site clearance uses actual gathering labor. Free keep placement is the founding clearance exception.
- A normal building needs four delivered lumber and 45 worker-minutes of construction labor, with movement inside the site. Construction accepts multiple builders; each attending worker adds their elapsed labor to the same progress, so additional workers accelerate completion. Site clearance can assign different natural features concurrently. Crews on a compact dirt-road run share its current tile rather than transferring labor across tiles. Dirt roads need no materials and two worker-minutes per tile. The keep is immediate and free.
- Demolition returns half the construction recipe, currently two lumber for ordinary buildings. Free roads and keeps refund no construction materials. Stored contents and delivered materials remain physical goods.
- Groundpiles merge on the same tile up to 100 units per pile, may sit on roads, and do not block walking. Empty piles are removed. Citizens carry up to four units per trip.
- Ordinary hauling stays local (24 tiles). Stockpile employees deliver exclusively to their assigned stockpile. Their eligible nearby piles/output have a 30-minute preference before unemployed fallback can assist. That preference applies only while an employee is working; unemployed citizens can cover off-duty stockpiles immediately. Full destinations cannot reserve more goods. Distant material collection is permitted for construction; food searches expand beyond the local area.
- Fishery production uses actual attending workers and exclusively claimed water. Each four water tiles support one effective worker; each effective worker can produce nine fish in a full twelve-hour shift, before travel/food interruptions. Full output storage halts production.
- Fishery preview reach follows the Godot area-scaling principle: eight tiles times the square root of footprint area / four. Prior completed fisheries and construction sites reserve their water first. Overlap is red and excluded from the new site's stored zone. Cancellation/demolition makes that water available to later placements; existing zones do not silently expand.

## Ownership and cleanup

`SettlementMap` owns logistics and the activity system. Citizens retain task state and carried quantities using value IDs. Logistics owns inventories and source/destination reservations. Every ordinary exit, cancellation, dismissal-related interruption, and death passes through common cleanup that releases reservations and preserves cargo as a groundpile.

Construction deliveries reserve actual site capacity, not a parallel global material counter. Completed construction consumes its site inventory once. Removed containers spill remaining goods. Goods shown in the HUD include groundpiles, containers, material at sites, and cargo in transit.

Route failures are temporary, bounded records of endpoint identity, origin, topology revision, and retry time. They do not hold pointers. Food and logistics reuse these records; failed work candidates rotate rather than permanently blocking later candidates. An unavailable meal never destroys a useful work task merely because hunger is high.

A map's structural revision is separate from presentation-only construction progress. Navigation, storage reconciliation, housing assignment, and the large infrastructure texture avoid rebuilding for every progress update.

## File organization

- `simulation/systems/SettlementActivitySystem.*`: lifecycle, schedule, execution, shared cleanup, production dispatch.
- `simulation/systems/SettlementActivitySelection.cpp`: feasible task selection and routing.
- `simulation/systems/SettlementCitizenNeeds.cpp`: need progression and home assignment.
- `world/settlements/SettlementLogistics.*`: physical inventories and reservations.
- `world/settlements/objects/SettlementObjectLifecycle.cpp`: construction and demolition changes.
- `world/settlements/objects/jobs/`: shared workplace definition and separate fishery, stockpile, wheat_farm, pastureland, and bakery folders. Fishery production/zone policy and stockpile collection policy live with their jobs. The other three jobs have staffing/storage definitions; their production recipes remain future work.

Local maps use the same simulation whether presented in the city view or left on the world screen. They bypass the older aggregate economy/population placeholders to avoid double production or invented residents. This prioritizes consistent outcomes; a cheaper simulation for large numbers of inactive maps remains future work and must preserve these inventory, need, and employment rules.

Validation stays in one additional scenario test file, with existing tests updated where the new accessible construction/outdoor-workplace behavior supersedes old expectations. Rendering checks use the real SDL renderers and disposable artifacts under the ignored build directory.

Citizen base movement is 0.75 tiles per game minute, a 50% increase; road speed modifiers still apply.

## September 5: food timing, sleep, seasons and law balance

Hunger now rises by 50 over 12 game hours (100/day), superseding the earlier 50/day rule. Every meal cycle chooses a citizen-specific random threshold between 50 and 70 hunger; at 70 the search is urgent and can interrupt sleep or hauling. Food must still be physically acquired, and unsuccessful searches preserve productive tasks. Starvation damage scales with depletion so a healthy citizen still reaches death at 100 hunger without food.

Citizens seek five hours of sleep per noon-to-noon day, keeping a night's quota together across midnight. Employed citizens sleep outside their work shift; nighttime is preferred, with off-duty catch-up naps when needed. Urgent food needs may interrupt rest, and the remaining sleep quota persists. Sleep is now allowed only inside the assigned house; homeless citizens and citizens unable to reach their home do not sleep outdoors. Sleep displays blue zZZ above the citizen.

Seasons share the game clock and last three days each: Summer, Spring, Autumn, Winter. Seasonal sunrise/sunset guides sleep. No weather, crop or temperature effects are implied yet.

Workday controls are now limited to 0-14 hours. Employed citizens have a daily happiness contribution of zero at 12 hours, -1 at 13, -2 at 14, and +1 per shorter hour capped at +3/day. Unemployed citizens receive no workday-law contribution. Other needs still affect total happiness.

Management windows can be dragged by their headers and ease back inside the viewport when released near an edge. Hover hints are delayed, concise, and positioned to remain visible.

## Accepted future recipe design - NOT IMPLEMENTED

Adjustable-size objects will use user-specified per-tile construction costs. Fixed-size objects will use fixed recipes. Exact quantities have not been supplied. Do not infer them or implement this system yet; current recipes remain in force until the user supplies the next instruction.


## September 5: home interiors, individual leisure and mixed recipes

Residents physically enter their assigned home's footprint and idle with short, varied indoor movements. Sleep stays inside that home. Each citizen receives an individual random five-hour window within the seasonal night and outside their work shift. Before resting, a citizen seeks food if their projected hunger would become urgent during the remaining sleep; urgent food still interrupts sleep if necessary. Interrupted rest retains its remaining quota. Houses assign up to four residents and show their names and vacancies; spawning and completed construction refresh assignments.

Houses now have a fixed recipe of 16 lumber and 8 stone. Construction inventories reserve outstanding amounts separately for each material, so concurrent deliveries cannot fill the site with excess lumber and exclude stone. Other current recipes are unchanged, and the future per-tile system remains deferred.

Employees take an individually staggered 30-minute break when at their workplace. Break travel is bounded by a route back to the original work position before the break ends; production attendance continues during the break. Mutual, low-priority conversations last 2–15 minutes after the pair meets, show the other citizen's name, and yield to food, work and unemployed player commands. SettlementLeisure.cpp owns these shared plans.

Fishery work positions are reachable shoreline land tiles adjacent to the fishery's exclusive production water. Workers hold their fishing positions and display a rod/line rather than wandering through the generic workplace footprint.

The debug console is now at most 360 by 420 pixels and has a normal-font Spawn citizen button beneath its textbox. It uses the same command dispatch as spawncitizens 1, including active-settlement behavior from the world screen.

Validation: updated existing simulation and recipe checks, including indoor wandering, assigned-home-only sleeping, different sleep windows, completed sleep quotas, mixed-material house building, and resource conservation. Existing test suite passes; the disposable SDL render harness also verified the compact console and its spawn-button command.


## September 5: building doors and reversible placement

Fixed building previews have a center-edge door, initially on the south side. E rotates the door counterclockwise and R clockwise, only while placing. The dark preview tile carries two outward chevrons. Adjustable structures require a locked footprint, a valid non-corner edge tile with an accessible exterior tile, then a separate confirmation click. A footprint must have at least one edge of three tiles to offer a non-corner door. Roads retain their original doorless placement.

Door coordinates belong to construction sites and completed objects, survive completion, and render as darker tiles. New structures cannot cover the exterior tile of an existing door. Programmatic legacy placements receive a default door. Home entry and exit use the actual chosen doorway; indoor-to-outdoor routes include each interior step instead of moving residents directly to the exterior. Residents may spend short leisure periods outside, return indoors afterward, and still leave for food or higher-priority activities.

Right-click reverses the placement stage: chosen door, locked footprint, active drag, then selected object. Bottom build options remain open through those steps; the next right-click closes them. Choosing an object no longer closes its category menu.

Validation: existing suite passes with new rotation, corner rejection, door selection/reversal, and step-by-step doorway crossing checks. SDL render checks confirm fixed preview rotation, adjustable door selection and dark completed doors. No new test files were added.


## September 5: door feedback, movable windows and world settlement flow

Management windows and inspection panels can be dragged from their body or buttons with a four-pixel movement threshold. Simple clicks retain their action; workplace name editing retains text input. Edge return remains enabled. Debug console text selection is preserved.

During adjustable-building door selection the footprint alone uses blue valid-edge tiles and red invalid tiles, including corners, interior tiles and blocked exterior access. The fishery collection overlay is suspended for this stage and returns after door selection/cancellation. Doors use brown fill and a dark brown frame in previews, sites and finished objects. Fishermen retain their outdoor shoreline work behavior. Citizen and population-average hunger colors remain green below 50, progressing through warning colors above it.

After entering the first city, the world HUD shares the top-left information and five management buttons. The top name is the polity; population is the sum of actual citizens in all owned settlements; the third row names the active settlement. World management panels have a blank information area and five decision slots. Only Economy's Found a New Settlement decision is currently active. City panel contents remain unchanged.

Founding another settlement selects a region, asks only for its name, prepares the local map before changing the world, creates it under the existing polity/culture, and enters it without changing the capital. Owned markers can be clicked to select the active settlement and double-clicked to enter; Play also enters the active settlement. Existing maps and their simulations are retained across switching. New settlements use the current C++ region size and founding population, rather than copying the Godot village-size distinction. Eligibility borrows the prototype's 48-tile capital radius, mostly-land region and non-overlap rules, and also excludes foreign-controlled region tiles. A landmass-connectivity restriction is not added in this pass.

Reference reviewed read-only: Godot scripts/session/GameSession.gd, found_player_village_and_show, and scripts/world/simulation/WorldPoliticalState.gd, is_player_village_region_eligible.

Verification uses the existing suite plus the ignored SDL harness: independent second-settlement map/citizens, preserved capital/map identity, inherited culture, active-context switching, 16 total citizens across two settlements, name confirmation, world decision dispatch, body dragging, and rendered door/world UI. No additional test files were introduced.


## September 5: large designation scheduling and rendering

The observed running scene had 14 citizens, two settlements, and a large road designation. The debug overlay attributed the stalls to citizen simulation (733 ms maximum observed before pausing); aggregate world simulation was negligible. A deterministic Debug-build stress case independently reproduced worse stalls: 8,460 fragmented road runs on a 384-square map, 14 citizens, and 120 simulation steps averaged 1,222 ms per step with a 19,011 ms worst step. Designation plus storage reconciliation took 178 ms.

The fixes preserve physical work and apply to every instantiated settlement, regardless of which screen or settlement is presented:

- Construction candidates come from shared 32-tile spatial buckets plus a rotating global fallback. Each worker examines a bounded subset instead of sorting every site. Sorting uses precomputed distances and ID lookup indexes, not repeated linear site searches.
- Gathering/demolition opportunities are compiled incrementally (2,048 targets per substep). Retirement and command cleanup each inspect at most 1,024 entries per substep. Completed targets fail authoritative validation immediately; deferred cleanup never authorizes nonexistent work. Player cancellation invalidates the board and target lookup immediately.
- Resource and demolition claims have direct lookups. A tree being cleared for construction cannot simultaneously be claimed through a gathering designation. Construction labor itself remains shared by multiple citizens.
- Construction clearing uses chunk resource counts and a resumable 512-unit scan budget, skipping empty chunks instead of repeatedly walking a whole footprint. Searches retain cursors so distant or initially unreachable work is reconsidered.
- Object/site and inventory lookups use value IDs mapped to vector positions, with explicit structural invalidation. Roads allocate no empty construction inventory. Completing a road tile splits/trims only its own run and updates its ID indexes in place, preserving the other queued tiles and occupancy.
- A hard four-search activity limit supplements the existing time-credit rate and bounded A* node count. Saved path credit cannot all be spent in one substep. Long time advances still progress through successive bounded substeps.
- Navigation avoids scanning completed objects on every unblocked neighbor. Placement prefix data and fishery previews invalidate on structural changes, not every construction-progress update.
- Natural-resource sprite masks are compiled once. Empty chunks need no texture; at most eight nonempty dirty chunks refresh per frame, with a rotating cursor. Existing textures stay visible until refreshed, so very large highlight changes can finish visually over several frames while their simulation designation is already effective.
- Construction rendering retains its pixel/line buffers and updates its existing texture. Unchanged partial-placement previews reuse blocked-row overlays. These caches use settlement instance identity and structural revision. Goods totals accumulate all displayed resources in one inventory/citizen pass.

Final representative checks on this machine (Debug configuration, not an FPS guarantee): the original 8,460-run case averaged about 1 ms, peaked around 4-5 ms, and took about 30 ms to designate. A 20,040-run case averaged about 2.2 ms with an 8.1 ms peak over 1,000 steps. A 304,704-tree designation with 14 citizens averaged 0.52 ms with a 1.8 ms peak; registering that unusually dense full-map selection still took about 154 ms. With 400 citizens, the same gathering case averaged 4.4 ms with a 28.4 ms peak. These are simulation times, not full-frame times.

The real Simulation integration harness ran two settlements with 20,040 road runs each, switched the presented settlement halfway through, and verified that both completed roads. It averaged 2.7 ms for their combined simulation, with a 26.7 ms peak in that run. The normal-renderer forest check's initial frame dropped from about 279 ms to 29 ms after bounded texture refresh. Initial map presentation and huge synchronous designation registration can still produce short hitches; this pass removes the reproduced multi-second scheduling freezes, not every possible frame-time spike.

Validation: the existing suite, existing SDL/UI/settlement integration harness, and disposable stress harnesses under ignored out/build. No new tracked test files. Focused checks cover clearing across empty chunks, immediately rejecting removed command targets, reservation/resource conservation, shared construction, road completion, and independent settlement progress.

Copilot suggestions were checked against the live code. Territory scans occur only when its cached content changes; the second pass needs the final centroid to choose an actual owned anchor tile. Debug text already refreshes only while its stats panel is visible. Vector size and grid dimension accessors are constant-time. Zoom-event powers and adjustable edge-scroll response were retained; no measured evidence justified approximating those controls.
