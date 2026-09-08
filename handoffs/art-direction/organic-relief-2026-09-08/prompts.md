# Built-in concept generation prompts

```javascript
// @exec: {"yield_time_ms": 1000, "max_output_tokens": 700}
const jobs = [
["hill-a","a broad low weathered foothill with two unequal overlapping rounded rises, earth exposed on slopes, sparse deep olive vegetation, irregular tapering edges"],
["hill-b","an elongated low rocky foothill shoulder with three gentle unequal folded ridges, earth and subdued moss, no isolated cone"],
["ridge-a","one connected low alpine mountain ridge with three unequal angular summits and branching rocky spurs, largest summit off-center, broad tapering foothill base"],
["ridge-b","one connected weathered mountain ridge with two asymmetrical summits, a shallow saddle and long branching shoulder, irregular footprint"]
];
await Promise.all(jobs.map(async([key,subject])=>{
const r=await tools.image_gen__imagegen({prompt:`Use case: stylized-concept. Asset: a single world-map relief sprite for Paladin, ${subject}. Elevated near-top-down view appropriate to a distant strategy map. Restrained chunky pixel art, broad simplified masses, dark rich earthy palette: #235747 #193E42 #74513F #4E3B39 #716D70 #596679 #394658 #886044 #49352F. Stone, soil and vegetation transition together within the silhouette. Lighting from upper left, modest contrast, no white highlights, no snow, no bright lime grass, no black outline, no repeated small decorations. Landscape 3:2 footprint. Isolated sprite on genuinely transparent background, no ground rectangle, no text, no cast shadow outside footprint. Source will be reduced to 64x40 art pixels; prioritize silhouette and large material masses. Future globe-map art, not a side-view platformer.`}); store(key,r);generatedImage(r);
}));

```

```javascript
// @exec: {"yield_time_ms": 120000, "max_output_tokens": 1000}
const r=await tools.image_gen__imagegen({referenced_image_paths:["C:/Users/Super/.codex/generated_images/01a07eea-6afb-7373-84d0-96a0afd072d8/exec-bb87c116-e9f6-4b39-86b6-69d673de4b69.png"],prompt:"Use case: precise-object-edit. Refine the attached approved Paladin mountain-range CONCEPT ART, preserving its attractive elevated view, asymmetrical connected two-summit composition, large left mountain and smaller right mountain, muted rich warm rock colors, dark colored shadows, and overall silhouette. Make the geology and ground transition more natural and organic: branching spurs that taper into rounded foothills, a believable irregular saddle and a shallow winding erosion gully rather than an apparent road, uneven patches of grass flowing up sheltered slopes and into crevices, more exposed stone on steep convex faces. Vegetation must form irregular clustered patches and broad quiet open areas, never evenly spaced rows or repeated shrub stamps. Low foothill surfaces should inherit the grass color of their surroundings and gradually reveal earth and then weathered rock as they rise, NOT have a fixed brown hill color or a sharp perimeter. Preserve beautiful readable large forms, reduce distracting fine noise, use clear deliberate pixel-art clusters, keep this suitable for later small strategic-world-map sprite conversion and a future globe view. Rich restrained Paladin palette only as closely as possible: #235747 #337A58 #193E42 #4F8C7A #74513F #4E3B39 #716D70 #596679 #394658 #886044 #49352F #A78D72. Warm earthy stone, no white snow, no bright yellow grass, no blue color cast. Very subtle upper-left illumination, colored dark shadows, no black outline. Isolate on genuinely transparent background, no dark background vignette or painted halo, no rectangular ground tile, no grid, no buildings, no icons, no text. Improve craft and organic transitions rather than adding detail everywhere."});store("ridge-refined",r);generatedImage(r);

```

```javascript
// @exec: {"yield_time_ms": 1000, "max_output_tokens": 700}
const jobs=[
["ridge-clean",{referenced_image_paths:["C:/Paladin/handoffs/art-direction/organic-relief-2026-09-08/source/ridge-refined.png"],prompt:"Use case background-extraction. Keep the mountain range artwork exactly unchanged. Remove all white and grey checkerboard background to actual transparent alpha. Preserve every mountain, vegetation and earth pixel, no redesign, no extra objects. Truly transparent background, no painted checkerboard, no halo."}],
["canopy-a",{prompt:"Use case stylized-concept. ONE isolated cluster of five unequal broadleaf tree crowns viewed directly from above for a distant world map. Extremely simplified chunky pixel-art shapes, no trunks, no individual leaves or highlights, irregular scalloped joined silhouette, a few broad shadow pockets. Palette #235747 #337A58 #193E42 only. Restrained rich dark green. Intended 16x12 pixels, read as a forest patch at tiny size, no busy texture, no grid. Genuine transparent alpha background, no ground tile, no glow, no checkerboard painting, no text."}],
["canopy-b",{prompt:"Use case stylized-concept. ONE isolated elongated patch of three loosely grouped unequal evergreen tree crowns, near-top-down distant world map view. Very simple chunky pixel-art silhouettes, compact angular canopy masses, no trunks, no fine leaf noise, broad dark shadows. Palette #235747 #337A58 #193E42 only. Rich subdued darker green. Intended 16x12 pixels, recognizably woodland at tiny scale. Genuine transparent alpha background, no ground rectangle, no halo, no painted checkerboard, no text."}]
];
await Promise.all(jobs.map(async([key,args])=>{const r=await tools.image_gen__imagegen(args);store(key,r);generatedImage(r);}));

```