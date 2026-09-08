# Livable tribal homes

The accepted v10 detail level is unchanged. House facades now have a readable timber-framed wooden entrance, an earthen threshold, a simple shuttered window, and one clay household jar. Rear and side faces use a shuttered window instead of a prison-like vent or blank wall. Plaster remains clean, coherent brown earth. These are modest inhabited homes, without glass, metal ornament, masonry, or noisy surface patches.

Only the house front/back/side catalog references and house door-opening region changed. The roof, global sprite sampling, grass, other building art, simulation, and rendering code were left unchanged. Both new runtime sprites are 96×28 PNGs and still pass through the existing shared 24-texels-per-tile importer. Wide facade artwork was composed to those proportions so doors and windows are not squashed during display.

Source: `source/house-facades.png`. Runtime: `C:/Paladin/assets/sprites/tribal-v11/house-front.png` and `house-back.png`. The earlier atlas in this folder is a proportion trial and is not the active export source. Generation used the built-in ImageGen tool and the v10 house artwork as reference. Prompt: two wide, low-detail mud-wall sprites with tall wooden door, thick simple timber lintel, square shuttered windows, earthen doorstep, one small clay jar; coherent earth palette, no fine grain or patchwork. Final source copied from `C:/Users/Super/.codex/generated_images/01a077eb-cbcd-7473-9710-020251c2b278/exec-a0d2cc97-dd2f-4b8d-b2c7-ed4f3c91c5ab.png`.

Reproduce with Windows PowerShell 5: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File handoffs/art-direction/tribal-v11/tools/export.ps1`. This reuses the v10 export helper and reproduces its unchanged base assets before exporting the two house facades. Exact earth/wood palette subset and binary alpha are preserved. Audit with the v10 `tools/audit.ps1`.

Before/after checksums confirmed that `SpriteStyle.h` and the v10 roof PNG are unchanged. Build and validation logs: `C:/Paladin/out/tribal-v11-build.log`, `tribal-v11-palette.log`, `tribal-v11-smoke.log`, and `tribal-v11-review.log`. Review captures include all four house facings and day/night lighting in `previews/`.

Verification passed: runtime packaging, regular application rendering/door checks, art-review scenario, and all 44 referenced PNGs against the 64-color palette with binary alpha. The packaged house-front checksum matches the source export. Reviewed the final in-game day capture; PNG previews are lossless conversions of the gameplay captures.
