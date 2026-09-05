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

After entering the first city, the world HUD shares the top-left information and five management buttons. The top name is the realm; population is the sum of actual citizens in all owned settlements; the third row names the active settlement. World management panels have a blank information area and five decision slots. Only Economy's Found a New Settlement decision is currently active. City panel contents remain unchanged.

Founding another settlement selects a region, asks only for its name, prepares the local map before changing the world, creates it under the existing realm/culture, and enters it without changing the capital. Owned markers can be clicked to select the active settlement and double-clicked to enter; Play also enters the active settlement. Existing maps and their simulations are retained across switching. New settlements use the current C++ region size and founding population, rather than copying the Godot village-size distinction. Eligibility borrows the prototype's 48-tile capital radius, mostly-land region and non-overlap rules, and also excludes foreign-controlled region tiles. A landmass-connectivity restriction is not added in this pass.

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


## September 5: placement colors and distinct sleeping positions

Construction outlines remain green in both material and labor states; readiness no longer turns road rows blue while workers alternate between sites. Object doors are a slightly darkened version of the object's own fill color, including placement previews. Only the small door cursor icon remains brown with a dark brown border. Door support is explicit in the object definition; roads, wheat farms and pastureland have none.

Residents reserve distinct sleeping tiles inside their assigned house, prefer clear positions away from the doorway, and physically walk there after entering. Sleep accrues only once they arrive. Duplicate legacy sleeping targets are reassigned; sharing is a last resort when every location is occupied/reserved. Indoor idle walks finish before sleep begins. Route replacement preserves the current partial step and plans from its endpoint, avoiding backward visual snaps when an idle citizen receives work.

Existing simulation checks cover separate indoor sleepers, doorless farm placement, and visual continuity when a partially moving idle citizen accepts construction work.


Sleep rollover correction: active sleep tasks now validate the current planned window as well as the remaining quota. The noon cycle reset can no longer renew an ongoing sleep task before the next night's window. Regression coverage first reproduced that rollover failure, then verified it wakes correctly; the existing household scenario now also checks that five-hour sleepers leave both the Sleep task and Sleeping presentation state.


Idle job-polling correction: queued work and inventory revisions no longer invalidate a citizen's home activity. Unemployed residents poll for executable work under the existing decision budget and switch only after successful assignment. A failed search leaves their route and activity intact, rather than repeatedly cancelling and restarting the trip home. The regression holds a resident stationary beside an unfunded construction order, verifies no movement during failed searches, then releases lumber and verifies that hauling starts. Hypothetical delivery-leg checks also clear the source leg's movement state before planning from the pickup endpoint.


## September 5: energy replaces scheduled daily sleep

This supersedes the fixed daily sleep quotas, scheduled windows, and indoor-only sleep rules above. The final corrected rate is **25 energy per 24 waking hours, plus 25 per 12 hours spent on work tasks** (not 75). Gathering, hauling, demolition, and construction incur the same extra effort drain for unemployed citizens; workplace tasks incur it during work hours. Breaks and leisure incur only the awake drain. Sleeping incurs no awake/work drain and restores 10 energy/hour, or 50 in five hours.

Citizens begin at 100 energy. Individual night-rest thresholds range from 50 to 60; off-duty citizens also seek daytime rest at 40, and severe exhaustion at 25 can override a shift. Rest is a continuous five-hour block measured only at the actual sleeping position. Urgent food can interrupt it. There is no midnight/noon reset and no forced sleep on spawning or at shift end. Below 50 energy, fatigue causes proportional health loss up to 20/day at zero energy; normal health recovery requires at least 50 energy.

Homes remain preferred, with separate reserved interior sleeping positions. Citizens without an accessible home search a bounded local area for an unoccupied outdoor sleeping position using the existing route budget. Both indoor and outdoor sleepers wake after their block. Seasonal night preference and food-before-sleep checks remain.

City population displays current population/completed housing capacity; the world total remains a population count. Citizen meters are Health, Happiness, Hunger, Energy. Placement remains selected after a successful construction order (except the unique founding keep). During door selection the footprint remains green, and only the hovered cell receives green/red validity feedback.


## September 5: corrected awake drain and post-work leisure

The latest correction replaces the 24-hour awake rate above: **16 waking hours cost 25 energy**, plus the unchanged 25 energy per 12 hours on work tasks, including unemployed gathering/hauling/construction. Sleep remains a five-hour block restoring 50; the player explicitly declined an eight-hour sleep window. Employed citizens reserve four hours after their shift for leisure before normal sleep, while critical exhaustion at 25 can override this preference. A normal block must fit before their next shift.

Health, Happiness and Energy bars now shrink from their right edge toward the left; Hunger still grows toward the right. Actual nearby conversation restores 0.1 happiness per game minute (0.2–1.5 per normal 2–15 minute conversation), with no reward for walking to meet. Unemployment adds a gentle two happiness points/day downward pressure, composed with the other happiness influences and clamped to 0–100.


### Flexible rest supersedes the fixed leisure window

The player subsequently requested a balanced, adjustable system instead of fixed times. Five hours is now a preferred recovery block used for planning, not a mandatory duration or ceiling. Citizens wake as soon as energy reaches 100, can recover longer when depleted, and return for their shift when above the fatigue threshold. Critical exhaustion can still override work. The leisure preference derives from the off-duty interval minus estimated recovery, with a tunable share allocated after work. Energy, the next shift's expected effort, and seasonal nighttime preference determine when rest is needed. Conversation gain, unemployment pressure, awake/work drains, recovery rate, fatigue thresholds and leisure share live together in CitizenSimulationPolicy.


## September 5: families, children, and Realm terminology

Founding/debug adults spawn with deterministic random ages 25–45 and pair with available opposite-sex unmarried adults. Marriage stores reciprocal citizen IDs; close parent/child and sibling pairings are excluded. The family allocator reserves homes for couples and their dependent children before filling spare beds with unmarried adults of either sex. A newborn takes precedence over adult lodgers, who leave through the actual door and become homeless if there is no other vacancy. Four family members fill a house; no additional birth occurs without a vacancy or an unmarried adult lodger to displace.

A configurable 3% daily probability is accumulated as eligible-time hazard, independent of presentation speed: both spouses must be alive, above 80 health, sharing a completed house, with the mother younger than 45. Newborns have parent IDs and the Child flag and render at half adult width/height. After one season (three days elapsed since birth), children become age-18 adults. Adult aging uses one year per four-season cycle (12 days), configurable separately. At age 45, mothers become ineligible before the next birth check. Children consume food, rest, socialize, and count in city/realm population, but cannot be hired, execute player work, or count as unemployed adults. Marriage and Child/Adult state appear in inspection.

SettlementFamilySystem owns aging, births, marriage maintenance and housing allocation. SettlementJobBoard owns command/construction indexing, clearing cursors, incremental maintenance and exclusive claims; decision/path budgets remain in SettlementActivitySystem. Household moves cancel activities through the same reservation-release authority. Citizen appends use stable IDs across vector growth and amortized capacity growth.

All source terminology, types, accessors, UI text and the Realm/RealmOrigin filenames now use Realm. The formatter now specifies namespace indentation, access-label alignment, and mandatory braces; first-party C++ files are normalized to that configuration.

Sleep initiation correction: energy must be strictly below 50 before a new sleep task can begin. Preparing for the next shift cannot override that gate. Individual normal night thresholds are now 45–50, daytime rest is reserved for greater fatigue, and ongoing sleep continues toward full recovery. This removes repeated 99-to-100 sleep attempts without introducing a fixed bedtime.
