# WOW recipe book (W26, MaskEhr's; LoadEhr W1 port)

LoadEhr W1: these are MaskEhr's Wow looks without the lasers (C1-C3, C27, C28 are gone; Infinity Corridor and Mirror Storm render
without their beams). In LoadEhr a look's model is a FILE a Model slot points at, by path (see `tools/w26_wow.py`); there is no
library. The clip / layer numbers below are MaskEhr's Arena clips. `tools/parity.py` renders every look here in MaskEhr and LoadEhr.

MaskEhr: twenty-one ready looks for the ten W26 effects, two per effect. They live in the **MaskEhr Wow** plugins: source
"MaskEhr Wow", effect "MaskEhr Wow FX". Resolume shows at most 512 FFGL params, so the W26 groups (+ Spots, Silhouette) are not
in MaskEhr / MaskEhr FX. In Arena each look is a clip on layer **"Wow"** (layer 26), columns 1-21 in table order. Clips are
untriggered. Projector Haze (C4) is an Abstract Field generator with the MaskEhr Wow FX effect: its input is the projected picture.

Source of truth: `LOOKS` in `tools/w26_wow.py`. `tools/w26_arena.py set|check <layer> <col> <look>` dials a look into a clip
over REST or compares it. Renders: `python3 tools/w26_wow.py run <tag>` (1 / 2 / 3 / 5 s at 1080p over the textured background).
Contact sheet: `verify/w26/split/contact.png`. Per-effect strips: `verify/w26/s<stage>/<look>/strip.png`.

Every effect is off by default. Smoke / haze looks need Smoke Amount or Haze up (it is in the recipes): the projector
beam shows only in a medium. Beat-driven looks (Strike / Release / Crack / Burst On Beat) need Arena's BPM running.

| Clip | Look | Stage | Model | Dials (non-default) |
|---|---|---|---|---|
| C4 | Projector Haze | 2 | Masks/Plague Mask (MaskEhr Wow FX on a generator) | Proj Enable On, Proj Source Input, Proj Pos X 0, Proj Pos Y 0.6, Proj Pos Z -4, Proj Throw 24, Proj Intensity 25, Proj Beam 1.2, Haze 0.25, Smoke Amount 1.2, Smoke Speed 0.25, Orbit Yaw 15, Key Intensity 1.5. |
| C5 | Projection Mapped | 2 | Skulls/Human Skull | Proj Image projector_test.png, Proj Enable On, Proj Pos X 1.2, Proj Pos Y 0.8, Proj Pos Z 4.5, Proj Throw 18, Proj Intensity 60, Proj Surface 1.5, Proj Beam 0.6, Haze 0.15, Smoke Amount 0.8, Key Intensity 0.6, Env Intensity 0.4, Orbit Yaw -25. |
| C6 | Eyes Of Fire | 3 | Skulls/Human Skull | Fire 1, Smoke Engine Sim + Detail, Sim Res 96, Emit Eyes 5, Eyes Heat 2, Emit Offset 0.1, Burn Rate 3, Flame Temp 2400, Flame Height 0.5, Flame Glow 3, Fire Smoke 0.4, Haze 0, Glow Cloud 0, Vorticity 4, Emit Eyes 4, Smoke Amount 2, Key Intensity 0.8, Env Intensity 0.4, Fire Light 3, Orbit Yaw 15. |
| C7 | Burning Letters | 3 | Text | Model Source Text, Text MASKEHR, Metal Override On, Brightness 0.35, Roughness Scale 0.6, Fire 1, Smoke Engine Sim + Detail, Sim Res 96, Sim Box 3, Emit Rim 2.5, Rim Heat 2, Emit Offset 0.1, Flame Temp 1800, Flame Height 0.6, Vorticity 4, Fire Smoke 0.6, Haze 0, Glow Cloud 0, Smoke Amount 2, Fire Burst Every 2, Burst Amount 4, Fire Light 3, Key Intensity 0.6. |
| C8 | Liquid Morph | 4 | Skulls/Human Skull | Model B Helmets/Diving Helmet/scene.gltf, Transition Style Liquid, Liquid Goo 0.6, Liquid Look Chrome, Liquid Wobble 0.4, Orbit Yaw 20, Transition 0.3. |
| C9 | Liquid Paint | 4 | Masks/Plague Mask | Model B Skulls/Human Skull/scene.gltf, Transition Style Liquid, Liquid Goo 0.8, Liquid Look Paint, Liquid Wobble 0.6, Transition 0.3. |
| C10 | Lightning Eyes | 5 | Skulls/Human Skull | Bolt Path Eye To Eye, Strike On Beat Every Beat, Bolt Count 2, Branching 0.6, Jaggedness 0.5, Bolt Life 0.45, Bolt Glow 3, Bolt Color H 0.62 S 0.6 B 1 A 1, Bolt Flash 3, Smoke Amount 1.2, Haze 0.1, Key Intensity 0.7, Env Intensity 0.3. |
| C11 | Tesla Skull | 5 | Skulls/Human Skull | Bolt Path Crawl, Strike Rate 5, Bolt Count 2, Branching 0.5, Jaggedness 0.45, Bolt Life 0.3, Bolt Glow 3, Bolt Color H 0.55 S 0.5 B 1 A 1, Bolt Flash 2, Key Intensity 0.6, Env Intensity 0.3, Orbit Yaw -20, Spin Y 6. |
| C12 | Firefly Face | 6 | Skulls/Human Skull | Swarm Count 20k, Mask Visible Off, Release On Beat Every 8, Release Time 1.5, Attraction 3, Flock 1, Swarm Speed 1, Firefly Size 2, Firefly Glow 3, Twinkle 0.5, Firefly Color H 0.14 S 0.75 B 1 A 1, Spin Y 6. |
| C13 | Firefly Halo | 6 | Masks/Plague Mask | Swarm Count 10k, Swarm Offset 0.35, Attraction 1.2, Flock 1.5, Swarm Speed 2, Firefly Size 2.5, Firefly Glow 4, Firefly Color H 0.5 S 0.6 B 1 A 1, Key Intensity 1.2. |
| C14 | Lava Cracks | 7 | Skulls/Human Skull | Lava Cracks 1, Crack Time 2.5, Crack Scale 2.5, Crack Width 0.35, Lava Temp 1700, Lava Glow 3, Heat Haze 1, Key Intensity 0.7, Env Intensity 0.35, Brightness 0.6, Orbit Yaw 15. Press: Crack Start. |
| C15 | Lava Split | 7 | Skulls/Human Skull | Lava Cracks 1, Crack Time 2, Crack Scale 2, Crack Width 0.4, Lava Temp 1800, Lava Glow 3, Lava Split 0.7, Lava Light 4, Heat Haze 1, Haze 0.25, Smoke Amount 0.6, Key Intensity 0.6, Env Intensity 0.3, Brightness 0.6, Orbit Yaw 25, Spin Y 5. Press: Crack Start. |
| C16 | Caustic Glass | 8 | Skulls/Human Skull | Glass 1, Glass IOR 1.5, Caustics 2, Caustic Source Whole Mask, Caustic IOR 1.5, Caustic Rainbow 0.8, Caustic Sharp 0.5, Caustic Ripple 0.25, Caustic Floor 1.2, Caustic Smoke 0.4, Caustic Surface 1, Key Elevation 40, Key Azimuth 70, Shadow Catcher 0.5, Haze 0.12, Spin Y 6. |
| C17 | Caustic Crystal | 8 | Stage Icons/Crystal Cluster | Caustics 2, Caustic Source Whole Mask, Caustic IOR 1.6, Caustic Rainbow 1.5, Caustic Sharp 0.4, Caustic Ripple 0.15, Caustic Floor 1.5, Caustic Smoke 0.6, Key Elevation 45, Key Azimuth -60, Shadow Catcher 0.5, Haze 0.15, Spin Y 5. |
| C18 | Bounce Shards | 9 | Skulls/Human Skull | Shatter Physics Bounce, Bounciness 0.55, Floor Friction 0.3, Shard Count 96, Shatter Force 1.5, Shatter Gravity 6, Shard Spin 4, Reform Time 1.2, Shadow Catcher 0.6, Mirror Floor 0.35, Orbit Yaw 25. Press: Shatter. |
| C19 | Shard Wall | 9 | Masks/Plague Mask | Shatter Physics Bounce, Bounciness 0.7, Floor Friction 0.2, Shard Count 128, Shatter Force 3, Shatter Gravity 4, Catcher Plane Wall, Wall Distance 1.2, Shadow Catcher 0.6, Orbit Yaw 55, Orbit Pitch 10. Press: Shatter. |
| C20 | Mirror Room | 10 | Skulls/Human Skull | Mirror Room Box, Mirror Count 3, Box Size 1.5, Box Depth 1.5, Mirror Color H 0.55 S 0.12 B 1 A 0.8, Mirror Gap 1.2, LED Colors Rainbow, LED Speed 0.25, Distance 8, Orbit Yaw 35, Orbit Pitch 18, Spin Y 6, Key Intensity 2. |
| C21 | Infinity Corridor | 10 | Masks/Plague Mask | Mirror Room Corridor, Mirror Count 4, Box Size 1.6, Box Depth 1.6, Mirror Color H 0.6 S 0.25 B 1 A 0.8, Mirror Gap 1, LED Colors Rainbow, LED Speed 0.4, Orbit Yaw 18, Orbit Pitch 6. (LoadEhr: without its lasers.) |


## W28 fixes (Amrit's review, 2026-10-02/03)

Fresh clips next to the originals on layer **Wow**, columns 27-34, untriggered. Each = the W26 look named in the third column
+ these dials (later win). Source of truth: `FIXES` in `tools/w28_review.py`; `w28_review.py arena set|check 26 <col> <look>`.
Every clip in Arena now moves at its dials' pace (W28 clock fix: Arena's clock is in milliseconds; it ran 6-12x fast before).
Liquid Morph 2 / Liquid Paint 2 replay the morph on EVERY click, a re-click while playing too (Arena-side animation, below).

| Clip | Look | Base + W28 dials |
|---|---|---|
| C29 | Burning Letters 2 | Burning Letters + Fire Speed 0.45, Burn Rate 1.6, Burst Amount 1.5, Fire Burst Every 4. |
| C30 | Liquid Morph 2 | Liquid Morph + Transition Time 6, Liquid Speed 0.25; Transition Play Off in Arena (harness renders: Once On Trigger). Arena: Transition = timeline animation 6 s (Play Once & Clear, re-trigger Restart), clip transport Play Once & Hold 6 s. |
| C31 | Liquid Paint 2 | Liquid Paint + Transition Time 7, Liquid Speed 0.2; Transition Play Off in Arena (harness renders: Once On Trigger). Arena: Transition = timeline animation 7 s (Play Once & Clear, re-trigger Restart), clip transport Play Once & Hold 7 s. |
| C32 | Bounce Shards 2 | Bounce Shards + Physics Speed 0.35. |
| C33 | Shard Wall 2 | Shard Wall + Physics Speed 0.35. |
| C34 | Lava Split 2 | Lava Split + Spin Y 3 (no press needed: a trigger spreads the cracks, they stay until Crack Reverse). |
