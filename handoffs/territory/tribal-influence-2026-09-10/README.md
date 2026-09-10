# Tribal influence territory — 2026-09-10

## Status and objective

This pass separates tribal political authority from civic sovereignty. Civic realms continue to use `TerritoryMap`, one authoritative controller per world tile. Tribal realms no longer write controlled tiles once their origin is known; instead they derive a continuous, overlapping influence field from populated power centers.

The central invariant is: **a tribal border is not stored as a border.** It is the visible result of competing continuous influence fields. Weak frontiers may overlap and fade into wilderness. Where two strong fields collide, the color transition narrows smoothly toward a strict-looking frontier without any hardcoded segment state.

## Power-center equation

For a populated tribal settlement with population `P`, reference population `P0`, and capital indicator `c`, its effective reach is

```
R(P) = m(c) * [ R0 + Rp * log2(1 + P / P0) ]
```

where the default policy uses `P0 = 100`, `R0 = 9 tiles`, `Rp = 5.5 tiles`, and `m(c) = 1.20` for a capital or `1.0` otherwise.

This logarithmic growth lets population matter strongly without allowing a tenfold population increase to produce a tenfold radius. Territorial *area* can still grow dramatically because radius grows in two dimensions.

Peak authority is

```
A(P) = clamp(1 - exp(-sqrt(P / P0)) + capitalBonus, 0, 1)
```

with a default capital bonus of `0.08`. Small settlements therefore remain visibly weak even at their center, while large population centers asymptotically approach full authority.

## Terrain-aware distance

The distance `D_s(x)` from power center `s` to world position `x` is not Euclidean distance. It is the least-cost path across the world grid using eight-neighbor movement:

- lowland land resistance: `1.00`
- hills resistance: `1.18`
- mountain resistance: `1.65`
- water: impassable
- east/west wrap: enabled
- north/south wrap: disabled

A step cost is its geometric step length (1 or sqrt(2)) times the mean resistance of the source and destination cells. This lets influence bend around terrain and prevents political color from radiating straight through oceans.

## Smooth compact influence kernel

For normalized least-cost distance `q = D_s(x) / R(P)`, the settlement signal is

```
K(q) = 1 - 10q^3 + 15q^4 - 6q^5,  0 <= q < 1
K(q) = 0,                              q >= 1

I_s(x) = A(P) * K(q)
```

The compact quintic kernel reaches both zero value and zero slope at its frontier. A lone tribal realm therefore fades out smoothly instead of ending on a tile edge.

A realm's signal at a point is the **maximum** signal from its power centers, rather than their sum:

```
I_R(x) = max_s I_s(x),  s belongs to realm R
```

This prevents several nearby settlements from producing artificial super-strength simply by stacking additive fields. Future outposts, camps, towns, sacred centers, military posts, or other sources can feed the same power-center model with their own strength policy.

## Emergent tribal-vs-tribal frontier

The field retains the strongest two realm signals at each location. Let `I1 >= I2` be those signals. The weaker local signal `I2` determines continuous contact pressure:

```
p = smoothstep(firmBegin, firmFull, I2)
```

Defaults are `firmBegin = 0.24` and `firmFull = 0.62`.

The competition exponent is

```
gamma = 1 + (gammaMax - 1) * p^2
```

with `gammaMax = 12`.

The primary realm's visual share in overlapping territory is then

```
w1 = I1^gamma / (I1^gamma + I2^gamma)
w2 = 1 - w1
```

When both fields are weak, `gamma` remains near 1 and the colors can overlap broadly. As both fields become strong, `gamma` rises continuously and the same underlying field produces a much sharper transition. No boolean `formal_border`, no stored border segment, and no phase switch exists.

This means one part of the same tribal frontier can become crisp because populations and power are high there while a distant flank remains diffuse and overlapping.

## Civic relationship

`Realm::usesTribalInfluence()` is true for origin `tribal`; `usesCivicControl()` is true for origin `civic`.

Once a tribal capital is established:

- any provisional controller cells for that realm are cleared;
- tribal settlements stop writing `TerritoryMap` cells;
- founding checks continue to respect civic `TerritoryMap` control but do not treat tribal influence as binary exclusion;
- moving a sole tribal capital moves the power center instead of rebuilding controller tiles.

Changing identity from civic to tribal clears that realm's discrete sovereignty. Changing from tribal to civic materializes civic controlled territory around each owned settlement using the existing `TerritoryFoundationSystem` budgets.

Thus the distinction is simulation-level, not merely a shader choice.

## Rendering

`WorldRealmPresentationRenderer` preserves the existing `WorldRenderer` call sites and composes political presentation in this order:

1. continuous tribal influence;
2. existing civic realm fill and strict border rendering;
3. subtle inward civic edge relief.

Civic cells mask tribal color, so a tribal/civic boundary is clear because the civic state's formal sovereignty wins the presentation boundary.

Tribal influence is sampled at two presentation samples per logical world tile. It uses no explicit line border. Low influence controls both opacity and a restrained darker-edge shade, producing the requested fade away from power centers. Strong overlap sharpens only through the competition equation above.

Civic territory receives a shallow five-tile inward distance shade with a maximum dark overlay alpha of 36/255. This is intentionally gentler than CK3-style beveling and is meant to approach the quieter territorial depth of Victoria II while leaving the existing civic colors and border geometry intact.

Tribal labels are derived from the actual primary-influence footprint using circular longitude averaging, so they remain valid even though tribal realms have no controlled tiles and can cross the east/west seam.

## Cache and performance model

`TribalInfluenceMap` is lazily rebuilt. Its source signature includes:

- world terrain revision;
- all influence-policy constants;
- realm IDs, origins, and capitals;
- settlement IDs, owners, positions, populations, and population versions.

The presentation texture is likewise cached by influence revision, civic territory revision, and realm visual signature. Camera movement does not recompute political fields.

The simulation field stores only the strongest two realm signals per cell. It does not retain one full float raster per realm.

## Tests / required invariants

The new regression checks cover:

- population increases both reach and authority;
- the compact kernel is monotonic and reaches zero at its support edge;
- direct tribal capitals produce no binary controller cells;
- influence decreases away from a power center;
- stronger population expands the field and increments its revision;
- two tribal fields can coexist at the same world cell;
- stronger mutual contact increases the competition exponent;
- water forms a true propagation barrier;
- civic -> tribal removes discrete control;
- tribal -> civic restores discrete sovereignty;
- switching back to tribal restores influence and removes controlled tiles.

## Deferred extensions

Not hardcoded in this pass:

- outpost/camp/sacred-site/military influence source classes;
- cultural affinity or hostility modifiers;
- roads/rivers as propagation accelerators;
- military occupation or raiding pressure;
- diplomatic recognition of frontiers;
- explicit legal claims separate from practical authority;
- player UI for inspecting competing influence values.

These should extend the continuous source/propagation model rather than introducing stored tribal border segments.
