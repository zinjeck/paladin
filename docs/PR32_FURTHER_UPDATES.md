# PR32: Further Updates completion

Base: `a662dc2cd711acd7d0eaa8b6a6c53389fa9e8248`, the September 21 PC-matching snapshot. This pass preserves its implementation instead of rebuilding the interrupted work from PR31.

## Existing behavior retained

The catalog contains named goods, not generic `food` or `materials` inventory. Strategic economies use hourly steps; geography, production capacity, stocks and construction affect demand. Named foods supply military rations. Commercial foreign caravans are supported: an active treaty, in-range partner, reachable land route, seller inventory and buyer money are required. Exporters do not need their own money. These statements supersede older PR30/PR31 notes describing trade cargo as own-settlement-only/future work.

## Remaining defects addressed

`Application::clearSettlementInspection` clears selection, inspector layout and embedded trade input together. All dismissal paths use it immediately, so a queued release cannot operate an invisible depot before the next frame.

Strategic AI commerce checks eligible suppliers beyond the eight nearest cities. Aggregate quote ranking remains separate from route finding: one buyer and at most one market route search per tick.

Patrols continue to transfer existing canonical soldiers and supplies out of garrisons. Destination distance and local walks respect longitude wrapping. A failed friendly-city route falls back to one short, step-validated dry-land walk. The shared budget permits only one patrol planning decision per tick, with at most one city search plus one short fallback; there is no per-city full-world search loop.

## Regression and visual evidence

`Pr32ContinuationChecks.h` covers paid delivery of lumber, stone, coal, iron, bread and fish past eight unsuitable partners; exporter liquidity; nonfood cargo/money conservation; patrol personnel/supply conservation; impassable destinations; and the longitude seam. Existing PR30 tests cover the named-resource forecast and commercial-caravan housekeeping.

Application smoke checks dismiss a real city's depot between pointer-down and pointer-up, verify cleared hitboxes and keyboard focus before rendering, and check Ledger and world-screen exits. CI requires `pr32-depot-selected.bmp`, `pr32-depot-dismissed.bmp`, `pr32-depot-ledger.bmp`, and `pr32-depot-world-exit.bmp`, plus the no-treaty/import/export and world-economy captures.

`tools/check_windows_icon.py` loads the executable as data, never runs its entry point, and compares every embedded icon image byte-for-byte with `assets/icons/paladin.ico`. Both Debug and Release CI run it; the Release JSON report is retained with the art artifact. No icon artwork or font files are changed.

Run results, exact tested head and any environment limitations belong in the PR completion report. Preserve all existing art and performance assertions, including the 80 ms software-globe worst-frame check.
