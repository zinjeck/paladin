# Settlement activity loop — September 5, 2026

This pass replaces the former attendance controller with a single settlement-owned activity authority. Employment records relationships; needs create urgency; the activity system alone selects and executes tasks. Movement executes routes and supplies bounded idle wandering when there is no activity.

## Current gameplay rules

- Founding the keep grants exactly 40 lumber, 40 stone, and 20 fish once. Keep capacity is 100 total units. Stockpiles hold 250; other workplaces currently hold 50. Reservations share that total capacity across resources.
- Work and ordinary unemployed labor run from 08:00 through 16:00. Food seeking continues outside those hours. An already picked-up load can finish delivery after the shift.
- Hunger starts at zero and rises by 50 per 1,440 game minutes. At 50, a citizen seeks one physically accessible food unit, which removes 50 hunger. Carried edible cargo may supply a meal, reducing both cargo and its delivery reservation by one.
- Above 75 hunger, starvation damage rises linearly with severity. A healthy citizen loses 25 health by 87.5 hunger and reaches zero health at 100 hunger without food. Existing injuries can cause earlier death. Below 50 hunger, health recovers at 25 points/day.
- Happiness pressure comes from hunger, poor health, and accumulated homelessness. Homelessness begins gently and grows over days; housed, fed citizens can recover happiness. Completed houses automatically house four residents each.
- Trees and natural rocks yield four lumber and four stone respectively. Site clearance uses actual gathering labor. Free keep placement is the founding clearance exception.
- A normal building needs four delivered lumber and 45 worker-minutes of construction labor, with movement inside the site. Dirt roads need no materials and two worker-minutes per tile. The keep is immediate and free.
- Demolition returns half the construction recipe, currently two lumber for ordinary buildings. Free roads and keeps refund no construction materials. Stored contents and delivered materials remain physical goods.
- Groundpiles merge on the same tile up to 100 units per pile, may sit on roads, and do not block walking. Empty piles are removed. Citizens carry up to four units per trip.
- Ordinary hauling stays local (24 tiles). Stockpile employees deliver exclusively to their assigned stockpile. Their eligible nearby piles/output have a 30-minute preference before unemployed fallback can assist. Full destinations cannot reserve more goods. Distant material collection is permitted for construction; food searches expand beyond the local area.
- Fishery production uses actual attending workers and exclusively claimed water. Each four water tiles support one effective worker; each effective worker can produce six fish in a full eight-hour shift, before travel/food interruptions. Full output storage halts production.
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
