# Paladin art and rendering requirements

- Every world sprite must share the same final pixel size. The canonical grid is 16 art pixels per logical tile (`WorldPixelGrid.h`). City and world scenes must pass through `WorldPixelScene`, including terrain, fitted roofs, attachments, entities, animation and lighting. Never bypass this with a separately drawn high-resolution world sprite. HUD icons use native pixels without arbitrary rescaling.
- Before delivering rendering or artwork changes, build and pass the `PaladinArtCheck` target. It checks actual day/night output pixel blocks, sprite integration, and resource cutout contamination. Inspect close-up and normal-view screenshots as well; file dimensions and palette checks alone are insufficient.
- Use the approved palette in `config/art-palette.hex` for source sprites. Runtime lighting is separate. Day should be vivid and luminous with colored shadows; night cool and dark with distinct warm local lights. Reference direction: No Game No Life by day, Death Note by night.
- Tribal houses should look inhabited through details attached to their wall planes. Do not overlay free-standing pottery on the facade. Ground props must sit on the ground with convincing contact.
- Inspect each exported atlas object independently. Never assume equal atlas cells isolate the intended object: neighboring foliage previously leaked into the meat icon.
- Keep source art, generated material and review images under `handoffs/art-direction/`; runtime PNGs belong in `assets/sprites/`. Do not put art sources in `out/` or change the user's Krita originals. Build-packaged copies of assets are expected beside the executable.
- Preserve distance caching, bounded work and pause-controlled animation. Do not allocate new textures on every zoom step.
