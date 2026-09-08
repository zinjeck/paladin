# City depth, celestial backdrop and tree varieties

Local implementation, 2026-09-08. Existing source PNGs were not edited.

- Low cutaway walls reduced from 5/16 to 4/16 tile, retaining continuous side/front caps. Colored contact occlusion distinguishes the tall rear wall at its intersections, with broader receiving-surface shading on caps and floors.
- Violet-brown depth shading at plaster/beam joints, beneath eaves, beds, tables, jars/baskets, crates, fishing stations and storage shelters. Existing roof/tree shadow masks use a complementary violet tint; trees and rocks receive an additional cached cast silhouette plus their tight contact shadow. Existing citizen and animal shadows retained.
- Brighter world stars live on a fixed celestial sphere and rotate with globe-camera yaw/pitch. No new per-frame textures.
- City species selection: 6% birch and 12% conifer in plain/forest/hills, the remainder broadleaf. Taiga, tundra and polar tree visuals exclusively conifer. Stable selection from tile identity and map seed, with existing placement/resources unchanged. Birch bark and conifer crown textures are built once at load on the 16-pixel grid and packed into the foliage atlas. Approved palette only.

Validation: Release PaladinArtCheck and application smoke checks passed, including tree species proportions, northern exclusivity and existing building clearance. Debug and Release rebuilt. Hardware review passed. Warm dense-city rendering across sampled zooms averaged 0.61–2.35 ms, worst warm frame 3.52 ms; initial cold-load maximum 165.5 ms remains. No zero-stutter claim. Globe and city review images are in this folder.
