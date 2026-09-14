# Paladin character style — approved direction, version 1

Established 2026-09-14 from the user's approved chunky citizen/soldier concept and actual city screenshot. This document governs citizen and soldier artwork; it does not replace the shared environment art rules.

## Visual language

Small, compact people viewed from the city's elevated overhead camera. Head and shoulders carry the silhouette; faces are a few skin pixels, never miniature painted portraits. Use deliberate square pixel clusters, hard edges, simple clothing folds and readable tools. Preserve the game's grounded timber-and-thatch medieval character.

## Pixel grid and size

All runtime art uses the existing 16 art pixels per logical tile through WorldPixelScene. Use a 16 × 20 transparent export canvas, a consistent foot anchor and a compact body approximately 9 × 12 pixels. The intended increase over the existing compact person is only one art pixel, not a larger rendering pixel or a separately scaled high-resolution sprite. Weapons can extend beyond the body within the canvas. Review actual occupied silhouette and scene scale before accepting exports.

## Palette and lighting

The sole source-color authority is config/art-palette.hex. Every nontransparent exported pixel must match an entry exactly. Use a limited subset per character: warm linen, brown leather, muted green or blue cloth, gray iron, and two or three skin/hair shades. Use two or three tones per material; no baked sunlight, glow, smooth gradients or antialiasing. Runtime lighting supplies time-of-day changes. Transparent pixels have zero alpha; the renderer supplies ground shadows.

## Male and female variants

Both variants share body scale, role colors, equipment, armor coverage and professional readability. Male defaults have a short hair cluster; female defaults add a small longer back-hair or tied-hair cluster, particularly legible from behind. Hair stays inside the silhouette budget and practical around helmets. Do not use exaggerated anatomy or different armor protection to distinguish variants.

## Roles

Current workplaces: farmer (wheat_farm), fisher (fishing_grounds), logger (logging_grounds), herder (pastureland), baker (bakery), merchant (market), porter (stockpile). Builder represents construction; citizen supplies the unassigned fallback. Smith, herbalist and laborer preserve the approved concept as reserved artwork rather than inventing simulation professions.

Soldier artwork: militia, spearman, archer, swordsman, crossbowman, captain. Equipment identifies role; a restrained common green cloth family identifies the military. Rank is indicated by a small helmet or cloak accent. These art roles do not create military gameplay systems.

## Export and validation

Preserve generated sources, exact prompts and provenance here. Runtime PNGs belong in assets/sprites/characters-v1. Front and back variants require independent visual inspection for clipping and neighboring objects. Use fixed bounds and anchors; do not rebuild textures during zoom. Generated sheets are source art until individual exports pass palette, alpha, silhouette, catalog and PaladinArtCheck validation. Inspect normal and close city views under day and night lighting. Do not claim animation frames exist unless separately authored and verified.
