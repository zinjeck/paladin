# PR 25: shared world surface, stable cartography, solar optics

## Scope and baseline

Rendering-only completion of PR 25 on `fix/world-surface-cartography-stability`.
Baseline: `a01b41cbd14a84c5ce79837a7b89c9d68eb88219`, plus the PR's temporary
input-bundling workflow. That bootstrap workflow is removed by this completion.
No simulation, civic ownership, tribal influence equations, source PNGs, palettes,
city artwork, UI panel redesigns, or unrelated feature branches are changed.

## Replaced behavior

- Retired the independent square civic outline mesh and half-tile tribal/civic
  paint masks. They could not coincide with the displaced visible coastline.
- Replaced `WorldTerritoryPresentationRenderer` and `TribalInfluenceRenderer`
  presentation passes with one implementation behind the existing
  `WorldRealmPresentationRenderer` public entry points. Nothing writes controller
  cells or changes the population/power-center equations.
- Settlement symbols/names no longer enter a shrinking world-object framebuffer.
  The 10-22 native-pixel symbols and existing compact settlement-name range remain;
  realm-name sizing/contrast remains the same.
- Removed coarse per-icon snapping. The source camera and final terrain residual
  now belong to the whole world presentation, not independently rounded objects.

## Surface contract

`WorldPoliticalSurface.h` samples the exact `coastSample(..., true)` plus
`worldLandField` used by `GlobeRenderer`'s atlas. Visible land is field >= 0.5.
Among the dry interpolation contributors, civic ownership competes with other
civic realms and unclaimed dry land. Ocean contributes to coastline shape but
cannot steal the shore from an owned island. Civic and tribal paint are exclusive
at a civic boundary. Tribal visual interpolation retains the actual top-two
influence signals; it does not promote a tribe into binary sovereignty.

Fill, inward relief and civic ink are generated from that ONE sample mask.
Boundary ink stays on its dry, owned side. There are no separately positioned
outline endpoints. Uncontested tribes still have no outline. The 18% close fill,
22% close civic-border weight, and political/terrain map modes are preserved.

A coarse atlas has at most 2048 texels on its long dimension. Close presentation
uses 16x16-tile pages at the canonical 16 art pixels/tile. A one-texel halo samples
absolute coordinates so page edges join. At most four detail pages are built per
render call; at most 64 reside (32 MiB maximum RGBA fill + border payload).
Camera movement does not invalidate pages. Each surface patch contributes ONCE,
using either its fine page or coarse fallback, never alpha stacked atop both.
Terrain, controller, influence and realm-identity revisions invalidate together.

Flat terrain meshes use map-anchored nodes aligned to the actual target pixel
pitch. Removing all snapping exposed cracks in SDL software geometry, so the
final version snaps shared geometry to the target lattice, not arbitrary screen
coordinates. The political raster uses the same affine phase.

## Cartographic motion and compositing

Terrain and genuine world geometry still use 16 and 32 art pixels/tile respectively.
At close globe scale both render unrotated source charts, then apply the same roll,
overscan and residual. Overscan margin is a fixed bound at a given scale, not a
function of the current residual: otherwise every tiny pan changed raster scale.

Native annotations draw after those layers. Their local rectangles and name
plates remain stable; only their complete physical-pixel origins translate.
There is no screen-size divided by enlarged art-pixel pitch. Native symbols remain
upright under globe roll. The authoritative camera is never mutated by rendering.

Unit-pitch render targets can be forced when rotation/compositing requires them.
Rotations use the true viewport center as pivot, not the ceil-padded source
texture center. This removes a separate subtle terrain/annotation offset.

## Solar optics

The old corona stopped at radius 1 while radial bloom still had nonzero energy.
Only the old ray term was tapered, leaving a visible circular cutout.

The cached 1024px optical field now combines multi-scale bloom, unequal smooth
scatter/diffraction lobes, selected blue/violet wings, and a C2 envelope on ALL
radiance. Every ray has its own length and taper. Some weak ray ends are chromatic;
the hot core is near-white. This is an optical camera-response approximation,
not a photon simulation and not a replacement background PNG.

Textures remain cached, smoothly filtered, additive, and native-resolution.
Astronomy, legitimate offscreen projection, and solid-planet occlusion are kept.
A completely occulted source cannot leave a detached halo around the planet.
Unoccluded light uses one quad. Occlusion uses bounded integer scan rows to avoid
software-rendered scanline gaps from fractional-height strip geometry.

## Validation

`PaladinWorldSurfaceTests` is part of the mandatory `PaladinArtCheck` target.
The old application smoke/art checks remain; their engine object code is shared
through `PaladinSmokeCore` rather than compiled a third time.

Focused checks cover:
- Exact coast-mask agreement on displaced shores, an island, and a concave cove.
- Zero painted water samples in the software-rendered canonical coast fixture.
- Warm cache reuse and a terrain-edit invalidation, with bounded page memory.
- 480 native marker/name frames across flat/globe, roll, fractional zoom, and
  40/64/96/127.5/256 screen pixels per tile. Translated plate RGB must match exactly.
- No coarse 3+ pixel hops for the subpixel pan trajectory.
- Full flat/globe political and terrain captures with zero black geometry cracks.
- Solar edge energy, chromatic lobes, actual white core, no bloom scanline gaps,
  full occultation, and cache reuse during camera motion.
- Unmodified civic tile counts and no tribal controlled tiles after rendering.

CI requires the coast, four map-mode/projection images, two sun images and the
480-row motion CSV; missing evidence fails the same dedicated software-art job.

## Remaining boundaries

Parameter tuning remains art direction, not a physical accuracy claim. Raster
rotation and changes in zoom necessarily resample finite pixels; the exact
translated-plate guarantee applies to fixed-scale panning, as exercised above.
GPU-specific behavior should still be reviewed on the player's machine. No merge
is performed by this implementation; review the final PR checks before merging.
