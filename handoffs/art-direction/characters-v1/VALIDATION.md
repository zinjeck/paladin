# Character artwork v1 delivery

## Delivered

- 72 individual transparent PNGs: 12 civilian roles and 6 soldier roles, each with male/female front/back poses.
- Runtime files: `assets/sprites/characters-v1/`; all entries registered in `assets/sprites/sprites.catalog`.
- 16 x 20 canvases; occupied silhouettes 11–14 pixels tall including hats/tools, with consistent foot anchor. Adult sprites remain compact within one tile of body width.
- Official style: `STYLE.md`, referenced by repository `AGENTS.md`; convenient local overview in `docs/character-art-style.md`.
- Built-in image_gen source sheets, exact prompts, source hashes, export script, per-file SHA-256 manifest, isolated sprite review sheets and portable ZIP retained here.
- Employment selects seven existing workplace appearances; construction selects builder. Unassigned citizens and children use generic citizen artwork. Sex and current north/south movement select directional variants. No new simulation professions or military unit behavior were introduced.

## Checks completed

- All 72 exported objects inspected separately in the three labeled review sheets; no neighboring atlas objects in the crops.
- Independent PNG scan: 72 files, 6,065 opaque pixels, every color exactly in `config/art-palette.hex`, binary alpha, dimensions 16 x 20.
- Asset compiler: 296 sources, 9 packed atlas pages, zero errors and warnings.
- Final Release `PaladinArtCheck`: passed world-surface/solar checks and application art/routing checks. Added checks require every character variant to load.
- Actual city captures inspected at normal and close scale, day and night: `review/characters-{normal,close}-{day,night}.bmp`. Grounding, texture sampling and lighting use the existing city renderer.
- Release and Debug `Paladin.exe` built successfully. The standard `Play Paladin.cmd` launches the updated Release executable.
- `git diff --check` passed.

## Performance evidence and limits

Actual Debug executable, Direct3D11, 576 x 576 city, 120 frames per zoom/scroll phase, at 1x and 5x simulation speed. First run: zoom p95 5.58/3.83 ms and scroll p95 2.05/1.94 ms. Initial worst frame 876.98 ms. Repeat: zoom p95 7.23/5.69 ms and scroll p95 5.19/3.89 ms; initial worst frame 231.37 ms. The benchmark generated different worlds (32/42 realms), so this is a smoke profile, not a controlled before/after comparison or proof of the cause of cold loading stalls. No claim is made that the earlier world-map lag has been fixed.

These are static directional poses, not authored walking or combat animation cycles. East/west movement uses the front view. Soldier, smith, herbalist and laborer artwork is packaged and registered for future gameplay selection; it does not itself implement those simulation systems. Generated source artwork was normalized by deterministic chroma-key extraction, nearest-neighbor sampling and palette matching; no external provider or third-party artwork was used. The optional game-dev package CLI was unavailable, so this delivery was verified through Paladin's native asset compiler and rendering checks instead.
