# Debug console continuity

The tilde/grave key toggles the session console in both World and City. Right click never closes it. All console and stats text uses NormalFontRenderer. Output supports drag selection and Ctrl+A/C. The input supports selection, Ctrl+A/C/X/V, caret editing, and Up/Down history.

The stats panel is an explicit, user-requested exception to the grey-panel design: translucent black with blue accents. It has top-left minimize/restore and close controls, and scrolls. Keep other panels grey.

Console commands are parsed separately from presentation in debug/ConsoleCommand.h. Mutations target the presented player settlement ID, with capital fallback only when no presented ID exists. World view does not change that context. Detailed simulation tier is independent of presentation. Citizen spawning updates detailed records and aggregate population together, with stable settlement-local IDs, age 20, randomized sex, and no workplace assignment.

The local alphabetical command key is debug-console-commands.txt at the repository root; Git ignores it. Update that key whenever the user adds a command. Current commands: spawncitizens [number], stats. Input and output have bounded retention; spawning accepts up to 100000 citizens per command.

Stats report live C++ clock, timing, workload, resource, and cursor data; features that have not been implemented must be marked unavailable rather than given fabricated values. Refresh is throttled to 5 Hz and suspended when minimized or closed.

Local maps remain simulation-owned across views. The most recent city renderer stays cached across world/city transitions; camera positions are stored by settlement ID to support multiple player and AI settlements. First-time map generation still occurs when a settlement is first opened. Ownership checks remain in Simulation.

Navigation caches use the map instance ID plus object revision, never a retained map pointer. SettlementMap is explicitly noncopyable/nonmovable and receives a unique process-lifetime identity; replacing a map in reused storage cannot validate an old road cache. A regression test reconstructs a map at the same address and revision to verify this.
