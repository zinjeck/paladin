# Paladin city corrections - 2026-09-22

## Scope and accepted rules

This is a live local checkout change, based on HEAD 526e32d. No PR or commit was requested. The existing external desktop-control apparatus is unchanged; no automation hooks were added to game source.

The user's screenshots supersede the previous mine-polish pass. Large mountain/hill masses with a few winding navigable valleys are required, not high-frequency holes throughout every range. Caves remain, but they must be sparse and accessible. Mine service terraces retain the approved earth material; the pit remains the established gray/dark rock with narrow mineral seams.

The depot must never collect speculative export stock. A one-shot order means one exact batch. A standing order repeats exact batches, securing a buyer again for each batch. Import stock is separate: stockpiles buy it while any stockpile exists; otherwise households, markets and production workplaces can buy it through the normal payment paths.

## Implementation

- `SettlementRelief.h` defines low-frequency winding passes and bounded cave candidates. `SettlementMapGenerator.cpp` no longer applies the per-tile anisotropic ridge threshold that fragmented solid relief. World-derived biomes and resource data are preserved. Cave entrances begin at existing traversable land, require rock ahead, and do not run right through the mountain.
- Construction cancellation synchronizes delivered inventory before deleting its site, then uses the existing orphan-inventory ground-pile path. A common activity cleanup releases claims and drops carried cargo immediately, including while paused. Repeating a cancellation cannot duplicate refunds.
- `ApplicationCamera.cpp` no longer excludes the HUD from edge scrolling. Window focus, pointer bounds, dwell, and held-mouse-button guards remain; merely hovering a button or panel is not a blocker.
- Hunger distress starts at `foodSeekThreshold` (50 by default). Citizens below that threshold receive no HungerDistress penalty.
- Citizen death records are bounded, named and sequence-numbered. Local and military/strategic retirement paths record them once. Reports consume them before the ordinary ten-minute report cadence, independently of births, migration or deployment changes.
- `MineSurface.h` uses the shared framed adobe facade and full pitched thatch roof, with distinct wall and roof extents, a proportionate door/window, chimney and contact shadows. Hoist, loading rack and wheelbarrow have stable ground-contact anchors. The windlass rope/tub follows mining labor, not wall-clock time, and freezes while idle or paused. New mines require 7x7; compact presentation is retained for older small footprints. Shared mining service rows place workers and quarry entrances below the hut/equipment terrace without changing ore depletion accounting.
- `OutdoorGround.h` provides world-phase-aligned material patches with a broken native-pixel perimeter revealing the real grass beneath. It is used by mine terraces, outdoor compound floors, generic non-enclosed work grounds and stockpile ground. No stockpile roof/interior redesign was made. It clips work to the viewport and does not allocate zoom-step textures.
- Trade orders have stable IDs, persistence, fulfillment and collection-authorization state. Before collection, the existing shipment system validates a buyer, treaty, route and capacity and reserves buyer money in the shipment's single escrow balance. Inbound commitments count against capacity/demand. The worker fetches only the authorized resource deficit, including incoming reservations, and notices newly funded orders while idle. Local coarse replay obeys the same collection limit.
- The shipment remains the sole authority for cargo and escrow. Loading clears collection authorization. Delivery marks the one-shot fulfilled; its card disappears even while the empty caravan returns. Cancellation stops future work and refunds an unloaded commitment. Cargo already travelling is preserved through the existing shipment lifecycle rather than erased.
- The unused inspector column contains resource-icon order cards showing import/export, quantity, one-shot/standing, current status and Cancel. The list scrolls and cancellation is stable-ID-based, including when a card disappears between mouse-down and mouse-up.

## Tests and review artifacts

`CityCorrectionsChecks.h` is included in PaladinTests. It checks warm/cold site refunds, picked-up cargo cancellation, hunger boundaries, six 384x384 relief maps, reachable sparse caves, funded exact-batch shipments, cash/goods conservation, one-shot cleanup, cancellation, separate import eligibility, exactly-once named deaths and actual depot-worker hauling. The worker scenario first waits with no order, then sends exactly two iron once, then repeatedly sends batches of two under a standing order and stops after cancellation.

`PaladinCityCorrectionsVisualTests` is registered with CTest and PaladinArtCheck. It renders all four mine types at normal/close views and day/night, checks pause/work-driven hoist geometry, renders the generated relief overview, and drives real depot panel mouse events for a quantity of two, one-shot and standing cards, cancellation, and stale-click safety.

Review PNGs and logs remain here on the PC. They are not automatically exported to chat. The final build, full PaladinArtCheck target and full PaladinTests run all passed after the shared work-position alignment. Results are recorded in `final-build-art.log`, `final-build-art.exit`, `final-logic-tests.log`, and `final-logic-tests.exit`; both exit files contain zero. The full run also retained the existing world/city rendering, PR30/PR31 application, military/industry, shipment conservation, population and generation checks.

Visual inspection uses the actual CityRenderer/SDL image output, not image-hash changes or imagined window states. Reviewed output includes the mine facades and yard/grass transition, all four pit variants, night overview, large-range overview and populated/cancelled order cards. This is deterministic render/application-event testing, not a claim of an exhaustive manual playthrough.

## Runtime and limits

The rebuilt application is `out/build/x64-Debug/Paladin.exe`. Restart the executable for code changes. Generate a fresh world/city for terrain topology changes; this pass does not silently regenerate an already materialized city and overwrite its buildings or excavations. New mine placement has the larger minimum, while existing small objects remain renderable.

A funded buyer is secured at collection time, not a promise that later war, capture or terrain changes can never block a route. Such failures retain/refund cargo and money through the shipment system. Standing orders wait and retry when no eligible sale is available rather than draining local stock.
