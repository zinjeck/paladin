# Shared framed adobe walls — 2026-09-08

Implemented directly in C:/Paladin. All enclosed buildings using the modular wall family share timber corner/bay posts, lit post caps, braces, top and sill rails, and projecting worn-clay foundations. Existing plaster textures, palette, canonical 16 pixels/tile, wall footprints and interiors are retained.

Cutaways now compose a raised back wall with an inner plaster face, lower side sections with recessed clay cores and timber ends, and a low front sill. Door leaves and exterior wall attachments are omitted. Door openings retain their floor surface.

Validation: Release PaladinArtCheck passed, including door hiding for house, bakery and city_keep with all four entrance orientations. Hardware art review passed. Debug and Release executables rebuilt successfully, including the preceding sunshine zoom fade. git diff --check passed.

Review: tribal-day.bmp, tribal-night.bmp and tribal-interiors.bmp. Existing distant building overview rendering remains separate from the modular close-view presentation.
