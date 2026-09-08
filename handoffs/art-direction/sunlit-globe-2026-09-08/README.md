# Sunlit globe and synchronized city time

Implemented directly in C:/Paladin, 8 September 2026.

- Shared geographic solar incidence and continuous twilight/illumination curve for the globe and the active city's actual longitude and latitude. The city no longer uses the independent 05:00–21:00 daylight schedule. The shared clock remains authoritative, including pause.
- Directional diffuse sunlight now varies over the lit hemisphere, emphasizing spherical curvature. Night remains readable. City and globe retain their scene-specific night contrast and local lights while sharing sunrise/sunset timing.
- Subtle atmosphere at the horizon: a bounded 192-segment mesh, four radial falloff bands and one reused texel. Brightness follows sunlight. It stays inside the canonical world-pixel rendering pipeline; it does not blur terrain.
- Default main continent seeds increased from 3–5 to 5–6. Broad central cores and shorter, wider shoulders replace thin extended arms. Domain warping and flooded rifts are reduced, preserving substantial interiors and bays. The three regression seeds produce 4, 5 and 4 separate landmasses larger than 1% of map area; merging is still possible naturally.
- Existing seeds are deterministic under the updated algorithm; generate a new world to see the new continent layout.

Validation: PaladinArtCheck and all four Release CTest targets passed. Added directional-brightness, twilight and connected-landmass regressions. Inspected noon, terminator and near terrain captures. GPU timings are in gpu-review.log.

Release executable: C:/Paladin/out/build/x64-Release/Paladin.exe.
Debug compilation succeeded but linking was blocked by the running Debug game (LNK1168); replacement is pending closing that process. No running game was terminated.
