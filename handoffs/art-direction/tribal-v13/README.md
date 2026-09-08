# Tribal v13 — roads, terrain, and lived-in homes

Runtime exports: `C:/Paladin/assets/sprites/tribal-v13/`.
Generated sources and review captures remain here, outside build output.
Artist originals and the accepted v12 roofs are preserved.

## Appearance

- Roads use darker reddish earth, gravel, and worn patches. The road surface and its irregular blended edges share the material. House floors keep their separate, lighter material. Road caches retain the 32-pixel texture; caches are still bounded and prepared incrementally.
- Grass retains v12 pattern variation, replaces deep teal with emerald, and reduces the strongest highlights. Existing biome/temperature selection remains. Clumps are larger and bend away from nearby moving citizens, then recover. Their simulation-time clock freezes when paused. Contact checks use a visible-area spatial index; reactions are discarded after leaving view.
- Water uses royal blue and azure; shallows are lighter in both world and city views. Existing water animation and cool-night/warm-light presentation remain separate from the source palette.
- Darker rock surfaces support overlapping world peaks with stable variation. City mountain cores use Night depth (#080F1B). This is a presentation rule for existing mountain terrain, not a new mining/exploration simulation.
- Houses deterministically choose modest hanging/storage details. Roof-off interiors have four corner beds with eight blanket palettes, varied striped rugs and storage placement. The doorway and middle aisle remain clear. Bed assignments survive resident iteration order changes and are released on death or rehoming. Housed citizens route to their assigned beds to sleep; homeless citizens retain outdoor sleep.
- Homes start at level 1. The green upward-arrow button beside the level in the house panel consumes clicks without upgrading or dragging the panel.
- Completed objects, construction sites, groundpiles, and citizens receive a simple gold selection outline. Existing command and world selection systems remain in place.

F8 still toggles environment art. F6 reloads it. O toggles roofs.

## Export and verification

Run `tools/export.ps1` with Windows PowerShell to reproduce the runtime PNGs. The export uses nearest sampling, exact palette quantization, and binary alpha. All colors are checked against `config/art-palette.hex`; display lighting and edge fades may blend them.

Palette audit: 81 referenced PNGs, 114,420 pixels, all valid palette colors and alpha 0/255.
Core simulation tests pass, including bed assignment stability, released slots and sleep-at-bed assertions. Application smoke checks cover grass contact, pause/recovery, and the no-op home level control. Day/night, cutaway, home panel, zoom, and mountain/water previews are in `previews/`.

The software-rendered Debug performance fixture measured continuous zoom means of 62.4 ms over the built-up area and 57.1 ms over the forest; worst frames were 99.1/69.0 ms. These are diagnostic software-renderer timings, not hardware game FPS, and do not establish a performance improvement over v12.

## Generated source briefs

Both sources were made with the built-in image-generation tool, then exported locally; no CLI image API was used.

`source/details-atlas.png`: transparent 3-by-3 atlas of tribal reed awning, clay jars, firewood rack, hanging basket, hide on timber, simple shutters, straw bed with crimson blanket, and two dark slate mountain silhouettes. Moderate pixel detail for small game sprites; earthen timber/clay, recolorable bedding, dark slate mountains. Exported props are 24px, beds 24x32, peaks 64px. Awning and shutter exports are available in the catalog for later use; the present house selection uses basket, hide, jars, and wood rack.

`source/road.png`: seamless opaque top-down dark reddish-brown compacted earth road, small embedded gravel, subtle ruts and worn patches, restrained detail between Terraria and RimWorld; no grass or pavement border. Main soil #74513F, warm earth #7A5038, red clay #874D50, rut #4E3B39, gravel #95655F, sparse stone highlights #A99478. Final export samples a smaller source area so gravel survives at 32px.

