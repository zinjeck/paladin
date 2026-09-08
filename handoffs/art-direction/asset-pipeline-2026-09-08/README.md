# Compiled asset pipeline — 2026-09-08

Paladin now builds official source sprites into versioned `.palpak` packages. Source PNGs, catalogs and palette remain untouched. `src/assets/` is the SDL-free package/schema/manager layer; `tools/asset_compiler/` owns source import and deterministic packing; `src/rendering/AssetUpload.h` is the renderer upload boundary.

## Build and tools

Building `Paladin` automatically builds `PaladinAssetCompiler` and runs `PaladinCompileAssets`. Both Debug and Release consume `assets/packages` beside their executable. Current official output is four packages (`core`, `world`, `settlements`, `ui`), seven bounded 1024-pixel atlas pages, 490 records, 14,377,699 package bytes. The 225 catalog sprites, generated tree variants and four native UI icons preserve their existing import transforms.

Compiler commands (run the executable in either build directory):

```text
PaladinAssetCompiler compile C:/Paladin C:/Paladin/out/build/x64-Release/assets/packages
PaladinAssetCompiler validate C:/Paladin
PaladinAssetCompiler list <package-or-directory>
PaladinAssetCompiler stats <package-or-directory>
PaladinAssetCompiler inspect <package-or-directory> <canonical-name-or-decimal-id>
PaladinAssetCompiler clean C:/Paladin/.cache/assets
```

## Runtime and editing

Stable names use `paladin:` and explicit FNV-1a 64-bit IDs. Packages validate versions, endian marker, schema, checksums, bounds, duplicate IDs and dependency graphs. Sprite records reference atlas regions; logical texture views preserve existing crop, animation and mesh coordinates. The renderer shares uploaded pages across scene libraries and owns SDL texture creation/destruction.

Release's ordinary official-art path has no PNG decoding. Debug F6 fingerprints source inputs, incrementally imports changed content through the SHA-256 cache, remounts compiled packages and replaces sprite views. Invalid imports log a diagnostic and leave previously loaded views intact. Handles have globally unique mount generations so old generations cannot silently resolve after replacement. Small loose-source smoke fixtures use the same extracted importer only in development builds.

Optional packages in `assets/overrides/<layer-name>/` mount in sorted layer order after the base packages. Later layers win; equal-priority duplicate definitions, type-changing overrides and missing/cyclic dependencies are rejected. This provides layering, not a mod installation UI.

The deterministic cache includes source bytes, row metadata, palette, material substitutions, compiler/schema versions and the import implementation signature. Package manifests expose dependencies, reverse-dependent counts and output hashes. Unchanged package bytes are not rewritten. Legacy generated PNG copies were renamed to `sprites-source-backup` beside the builds for the package-only runtime check; authored sources remain in `C:/Paladin/assets/sprites`.

## Deliberate limits

Loading/upload is synchronous at startup; the CPU queue, residency states, usage stamps, memory accounting and generation handles provide the boundary for later asynchronous work. Packages are uncompressed, with indexed atlas payloads when lossless and RGBA otherwise. Mounted package bytes stay in memory. F6 can rebuild/reload whole affected packages; there is no file watcher or automatic memory-budget eviction. Fonts remain in the existing font pipeline. Future audio/localization types are defined but have no new importers in this sprite migration.

Recommended next improvement: reuse unchanged resident atlas pages across package reloads, then add a measured upload budget only if profiling justifies it. No second project was started.

## Validation

See `validation.txt` for final build and focused-check results. No broad simulation test sweep was added. Source changes remain local; no commits or PRs were created by this task.
