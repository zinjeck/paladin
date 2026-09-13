# Dynamic sun — 2026-09-13

The live sun now draws 24 independently animated soft rays using one cached beam texture and a fixed-size mesh. Each ray has distinct smooth length, width, angular and intensity phases. Its response also depends on the visible solar disc and the direction away from the planet edge. The underlying halo no longer contains the frozen ray image.

Halo size, atmospheric rim brightness and lens-ghost size/position/intensity also evolve gently. The existing world presentation clock controls all animation: speed scales it, pause freezes it. Astronomy still determines the sun's position; complete occlusion suppresses all direct rays. These remain photographic optical approximations.

Release and isolated Debug games rebuilt. Full PaladinArtCheck passed. The sun regression measures 23,011 changed pixels across three animation seconds, checks much smaller changes over 1/60 second, exact image equality at a held timestamp, unchanged texture-build count through 120 animated frames, and no direct light with a fully hidden sun. The solar pass averaged 3.149 ms in the software-renderer check.

Normal/close city day/night images, dynamic-sun-0.png versus dynamic-sun-3.png, and dynamic-sun-limb-0.png through dynamic-sun-limb-3.png were inspected. Existing city pixels, territory and founding checks pass.

Actual Debug Direct3D 11 camera benchmark: a generated 576x576 map with 42 realms and 168 settlements. At 1x, continuous zoom mean 3.141 ms / p95 5.070 ms, scroll mean 1.360 ms / p95 1.940 ms. At 5x, zoom mean 1.181 ms / p95 2.964 ms, scroll mean 1.155 ms / p95 1.611 ms. First-run cold asset warm-up reached 170.386 ms. This checks the real application renderer, not developed-city simulation load. No builds ran during profiling.

One earlier full-art attempt hit the existing 80 ms raw-globe software timing limit; the isolated rerun passed at 64.261 ms without modifying the threshold.

Run C:/Paladin/Play Paladin.cmd to use the updated Release game.

Release SHA-256: 25bd067ba4c8bcdc54371342ec54917de0e9f7bacaa9cc7e0b8e1a86b3c2b4c2
