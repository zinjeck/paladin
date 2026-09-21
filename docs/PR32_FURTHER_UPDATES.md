# PR32: Further Updates completion

Base: `a662dc2cd711acd7d0eaa8b6a6c53389fa9e8248`, the September 21 PC-matching snapshot. This pass preserves its implementation instead of rebuilding the interrupted work from PR31.

## Existing behavior retained and scope supersession

The catalog contains named goods, not generic `food` or `materials` inventory. The reports' aggregate food warning is not a tradable resource and remains valid. Strategic economies use hourly steps; geography, production capacity, stocks and modeled construction activity affect demand. Named foods supply military rations. Commercial foreign caravans require an active treaty, an in-range partner, a reachable land route, seller inventory and buyer money. Exporters do not need their own money.

These statements supersede the older PR30/PR31 and AGENTS descriptions of trade cargo as own-settlement-only or future work. Existing own-city shipments are still supported. Strategic AI production and construction demand are aggregate estimates, not a detailed simulation of every AI building. Patrol presence does not claim automatic invasion campaigns, tactical combat, sieges or conquest. Stockpile art remains outside this pass.

## Remaining defects addressed

`Application::clearSettlementInspection` clears selection, inspector layout and embedded trade input together. All dismissal paths use it immediately, so a queued release cannot operate an invisible depot before the next frame. This includes right-click, HUD/report changes, Ledger and city/world transitions.

The inspector background now renders after the city HUD, minimap and simulation controls, immediately below the embedded depot controls. This keeps it a single foreground surface rather than allowing the Goods HUD or minimap to show through beneath its controls. The base's world-economy graph spacing correction is retained.

Strategic AI commerce checks eligible suppliers beyond the eight nearest cities. Aggregate quote ranking remains separate from route finding: one buyer and at most one market route search per tick. Failed paths cannot debit inventory or treasury. The existing terrain-revision-keyed cache lets later attempts choose another supplier and permits a formerly blocked supplier again after terrain changes.

Patrols continue to transfer existing canonical soldiers and supplies out of garrisons. Destination distance and local walks respect longitude wrapping. A failed friendly-city route falls back to one short, step-validated dry-land walk. The shared budget permits only one patrol planning decision per tick, with at most one city search plus one short fallback; there is no per-city full-world search loop.

## Regression and visual evidence

`Pr32ContinuationChecks.h` verifies:

- Paid delivery of lumber, stone, coal, iron, bread and fish past eight unsuitable partners; zero-cash exporters; nonfood cargo/money conservation; and preservation of foreign commercial caravans during AI housekeeping.
- Failure of a better but unreachable supplier without payment or cargo loss, no repeated attempts while time is paused, delivery from a reachable backup, and restored access to the original supplier after terrain-revision invalidation. Completed shipment history may be pruned; canonical route IDs and conserved cargo are authoritative.
- Real geographic forecasts for six named goods: scarce stock creates demand, full stores remove demand and reduce price, and depletion restores demand. Local coal/iron production follows deposits and refreshes through the bounded forecast cursor when terrain changes.
- Patrol personnel/supply conservation, impassable friendly-city destinations and the longitude seam.

The compact test-only foundation policy isolates partner ranking from the unrelated nine-tile founding-spacing rule. Production defaults are unchanged.

Application smoke checks dismiss a real city's depot between pointer-down and pointer-up, verify cleared hitboxes and keyboard focus before rendering, and check Ledger and world-screen exits. CI requires `pr32-depot-selected.bmp`, `pr32-depot-dismissed.bmp`, `pr32-depot-ledger.bmp`, and `pr32-depot-world-exit.bmp`, plus the no-treaty/import/export and world-economy captures.

`tools/check_windows_icon.py` loads the executable as data, never runs its entry point, and compares every embedded icon image byte-for-byte with `assets/icons/paladin.ico`. Both Debug and Release CI run it; the Release JSON report is retained with the art artifact. No icon artwork or font files are changed.

## Validation procedure and limits

Build the application/tests, run CTest, run the mandatory `PaladinArtCheck` target, and separately run the actual application-input scenario with `PALADIN_SMOKE_UI_ONLY=1`. Inspect the real rendered depot/economy captures at normal scale and close up. The software-rendered scenes are automated regression fixtures, not a full manual playthrough on the user's GPU.

The independent full smoke test includes the existing 80 ms software-globe worst-frame assertion. Do not remove it, relax it, or describe targeted UI/art checks as proof of that bound. Machine-specific performance samples and any failing attempts must be distinguished from functional test results.

Exact tested head, final Windows workflow results, downloaded-artifact verification and environment-specific measurements are recorded in the PR completion report. Temporary source-publication workflows must not remain in the completed branch. Leave the PR open for user review; no automatic merge.
