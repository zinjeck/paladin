# UI, cities, characters and military implementation

User request 2026-09-14 is authorization; attached UI handoff is reference/design context. Work in C:/Paladin. Preserve all existing mechanics and canonical palette/pixel-grid requirements.

Required scope:
- Shared restrained medieval UI skin and legible type, matched to attached concept.
- Pixelated, coherent world-map settlement/town/city footprints with gradual growth, not random noise.
- Walking and gathering animations, crisp readable closest-zoom ZZZ annotations; pause/speed obey simulation presentation time.
- Barracks employment provides soldier pool; separate soldier/world unit entities, create/select/change soldier counts; city/world unit lists under Military; movement across world map.
- Army supply depot buys city food, produces rations; barracks buys rations; resources and transactions conserved. Rations edible only when normal food exhausted.
- Wheat/bread resources; gatherable wild wheat clumps across suitable terrain; wheat required for grain farm construction; wheat farm and bakery production; sparse smaller grain artwork.
- Simulation/routing/resource regression tests, full PaladinArtCheck, day/night normal/close visual review, actual Debug camera profile, rebuilt local executables.

Discovery: resource catalog currently fish/meat/food/materials/stone/lumber only. WheatFarm/Bakery only staffing/storage, no production. Army holds ID/realm/position only. UI Military routes to generic Ledger. Existing seven workplaces, citizen sprite export integration from previous turn now clean in repository. No modifications yet except this work record.
