project:    LoadEhr
pass:       w9-looks-pack-results
STATUS:     COMPLETE
parent:     981fdd5
result:     2a8e8eb 81ef24f 614d40f d8be013 791f14c 1d2dde3
session:    facfe9e7-9856-4f8e-aa05-41a20a5351d8
written_at: 2026-10-09T06:16:46Z

# LoadEhr W9 results: the looks pack (14 looks in 5 stages: Lines, Build, Cut, Surface Look, Shape)

Brief: `docs/handoffs/loadehr_2026-10-08_w9-looks-pack-brief.md`. Design notes (written before each stage's code): `docs/w9/design.md`.
Code commits (all pushed): A 2a8e8eb, B 81ef24f, C 614d40f, D d8be013, E 791f14c, clips tool 1d2dde3. This handoff is committed on top.

## Amrit's words
- Brief (2026-10-08): "Anything else you can think of that would look amazing in this plugin that mostly handles how the imported objects are
  handled?" On Cowork's list of 14: "Lets brief them all and then prune if something doesnt work." On W8: "Colors look good." (the HQ DXV
  comparison clips: accepted).
- This session: restarted Arena when asked ("Done, Arena restarted"; it had only quit, then "Arena is open now"; PID 27709 -> 15450).

## Bundles: where the room came from (no fifth bundle)
| | LoadEhr | FX | Wow | Wow FX |
|---|---|---|---|---|
| before W9 | 504 | 508 | 498 | 512 |
| A: Smoke Sim (30) -> Wow only, Project Input -> Wow FX, Face (10) -> LoadEhr FX, + Lines (5, classic) | 479 | 492 | 498 | 503 |
| B: + Build (4 classic + Build Axis everywhere) | 484 | 497 | 499 | 504 |
| C: + Cut (4, everywhere) | 488 | 501 | 503 | 508 |
| D: + Surface Look (5, classic) | 493 | 506 | 503 | 508 |
| E: + Shape Target (1, everywhere) | **494** | **507** | **504** | **509** |

Why: both FX carriers must fit 2 x shared + both sides' extras <= 1024 (it was 1020). 20 new controls needed at least 17 shared params to become
one-sided. Smoke Sim is what the Wow-only Trip and Fire are built on, so it went to the Wow side. The classic bundles keep the Noise smoke,
the Volume group and every light; in them the Smoke transition style now dissolves (as Swarm / Burn / Lava already did). Face went back to
LoadEhr FX (it was on Wow FX for room only). The new Lines / Build / Surface groups are classic (the object-handling side). Cut, Build Axis and
Shape Target are in all four bundles.
Saved-clip effect: a classic clip with Smoke Engine Sim, or a Wow FX clip with Face Track, keeps its values but runs that group at its default
(off).

## What shipped (per item: what it does, dials, method, cost)
Harness cost = 1080p GPU ms, skull / helmet (verified, harness only). Arena = the plugin's PERF line with the clip SELECTED (not triggered),
MBP M3 Max, Wire closed, 120 Hz ProMotion panel. "fps" is the delivered rate (1 / the median ProcessOpenGL interval). The full Arena table is at
the end. No GPU trace was taken.

**Two shared mechanisms.**
- The vertex + fragment looks live in the existing DEFORM twins, which now also get `#define DEFORM` in the FS. So they follow clones, letters,
  the morph pair, wire / points and shadows with no new program family. The plain programs are untouched: regress max 0.
- New CAPTURE twins run the model's own vertex stage at surface "spots" with transform feedback. That gives world position, normal and colour
  every frame, after every deform, morph and shard flight. Lines, spikes and copies are drawn from it.

**A. Lines (classic). Line Look / Amount / Reach / Speed / Audio. The look comes from the Wireframe group (Wire Color + Mix, Thickness, Glow / Bloom).**
1. **Plexus.** Points (200 + Amount^2 x 20k) from the Point Source set. Neighbours are found once, at rest, on a worker: a grid of cell = Reach,
   the 8 nearest per point. Cost of that step: 5k points -> 23k lines, 20k -> 94k, built in 9 ms or less. Lines fade with the current distance,
   so shards flying apart lose their lines. Speed = pulses running along the lines; Audio = 16 FFT bands, bass at the base.
   Harness: 5k 1.77 / 1.85, 20k 2.09 / 1.96 (solid 1.52 / 1.36). Arena: 94.6 fps, cpu 0.92 ms.
2. **Light Paint.** 20 + Amount^2 x 3000 heads leave world-space ribbons from a ring of past positions (60 samples / s, up to 4 s = Reach), so the
   camera can orbit the trails. Speed = each head circles its spot. Harness 1.71 / 1.42, top 1.88 / 1.97. Arena 91.9 fps.
3. **Neon Tubes.** The W5 feature edges as tubes in Wire Color with a white core. Light on the model: the tubes' glow is blurred wide (Reach) and
   multiplied onto the model's own picture. This is a screen-space approximation (no shadowing), chosen over a per-draw line-light term.
   Harness 3.80 / 3.04. Arena (text) 81.0 fps, cpu 1.55 ms.

**B. Build (classic; Build Axis everywhere). Build Look / Amount / Axis / Speed / On Beat. Progress-driven: scrubbing back retraces.**
4. **Slit-Scan.** A 2 s pose history (pose, deform phases, jelly), one entry per 1/60 s, kept in a texture buffer. Each vertex reads it at its own
   delay (Amount x 2 s x its height), so it is smooth with no tearing. Speed = waves of delay. Letter animation states are **not** in the history.
   Harness 1.78 / 1.70. Arena 100.8 fps.
5. **Print On.** A plane rises over the model's extent. The printed part shows layer ridges and a glowing head band, and its cut face is capped
   with the stencil buffer. The unprinted part shows in the Wire Mode's look (faint wire), or nothing. The main depth buffers are now
   DEPTH24_STENCIL8. Harness 1.93 / 2.13. Arena 85.1 fps.
6. **Exploded.** Letters (text / logos), else part slots, else islands, else 16 clusters for single-island scans. Pieces slide out from the
   centre; On Beat snaps them back and eases them out. Harness 1.63 / 1.78. Arena 88.1 fps.
14. **Black Hole** (the 4th Build Look, as the brief allowed). The model spirals into its centre, near points first, heating up. The post pass
   bends the whole picture with a point-mass thin lens and adds an event horizon (black, alpha 1) and a photon ring (light). In LoadEhr FX with
   Input Use Background, the input layer bends too. A source bundle cannot bend Arena's layers below it. Harness 3.43 / 3.48 (the post chain).
   Arena 91.8 fps.
   Strips 0 .. 1 .. 0 for Print On, Exploded and Black Hole: forward = backward, max 0.

**C. Cut (every bundle; the axis is Build Axis). Cut Look / Position / Size / Speed. A Cut look wins over Print On's cut.**
7. **Cross-Section.** A plane (Position; Speed sweeps it) with a slab of Cut Size under it. Both faces are capped with stencil and show a dark
   hatched slice with scan lines in Wire Color. Open / non-watertight scans: the parity goes wrong where a hole lets a ray miss a layer, giving
   stray or missing cap patches; the skull scan caps as a solid. Harness 2.41 / 2.69. Arena 97.6 fps, with sim smoke 86.8.
8. **Reveal Box.** The model is solid inside a rounded box (Size; Position rounds it into a sphere) that drifts on a closed path (Speed; Build On
   Beat hops it). The outside shows in the Wire Mode's look, or Wire Only when the mode is Off, with a glowing seam. Harness 4.24 / 2.95.
   Arena 96.6 fps.
9. **Window.** Discs at the eye-finder openings, set half a radius into the socket, plus the glass parts, are stenciled. A model with neither
   becomes a window as a whole silhouette. Behind them sits another world: another filled slot (Surf Source in the classic bundles), placed back
   (Position), bigger (Size) and turning (Speed). In the Wow bundles with Proj Enable, the projector's picture shows instead (video / DXV
   verified in the harness). Harness 1.66 / 2.29. Arena 97.9 fps (slot), 90.6 (video).

**D. Surface Look (classic). Surf Look / Amount / Scale / Speed / Source (Slot B / C / D / Text).** Spikes and copies stand on the dominant model.
10. **Audio Spikes.** Instanced cones (200 + Scale^2 x 6000) along the normal. Length follows the FFT band of the spike's height, clustered,
    dancing, glossy black. Harness 2.26 / 1.91. Arena 86.6 fps.
11. **Skin Of Copies.** A TBO-instanced variant of the main program (`progMainIT`) puts copies of Surf Source on the surface, wobbling; Text works
    through a hidden fifth slot. There is one LOD for the whole batch, chosen by the copies' size on screen, not a LOD per copy. Amount 1 .. 2
    dissolves the model under them. Harness 2.78 / 2.61. Arena 91.2 fps.
12. **Camera looks** (shading modes in the DEFORM twins, so they combine with everything).
    - X-Ray: additive, every layer. Harness 2.65 / 2.24, Arena 89.5.
    - Thermal: iron-bow ramp. Harness 1.84, Arena 95.2.
    - Night Vision. Harness 1.73, Arena 95.9.
    - LED Matrix: 4 .. 32 px pitch, transparent gaps. Harness 2.20, Arena 97.1.

**E. Shape (every bundle).**
13. **Shape Target** Sphere / Cube / Torus / Crystal: what Spherize morphs toward. Normals follow the shape. Sphere renders identically to W8's
    Spherize (max 0 at 0.3 / 0.7 / 1, skull + text). Strips forward = backward max 0. Harness about +0.1 ms. Arena 96.9 - 101.4 fps.

## Combinations shown (renders in verify/w9/*_combos)
- Plexus + Points, Light Paint + Twist, Neon + Wire Glow / Bloom.
- Print On + Wire Only (faint wire), Black Hole over the FX input, Exploded text + Letter Wave, Slit + Twist, Print On mid-hop.
- Cross-Section + sim smoke, Window + DXV video, Reveal + Points, Window mid-hop.
- Spikes + Twist, copies of text, X-Ray + Print On, LED + Plexus, Thermal + Exploded, Night Vision + Wire.
- Torus + Twist, Cube + Shatter, Crystal mid-hop.
- Mid-hop behaviour:
  - Lines: both models, crossfading over t 0.35 .. 0.65.
  - Build / Cut / camera looks: both models (the DEFORM twins).
  - Spikes, copies and the Window: the dominant model.

## Checks
- Regress max 0 on all 6 references, after every stage. Guards G1-G7 ok on all four bundles.
- W5 wire 48 / 48 max 0 and W6 points 28 / 28 max 0. Single reruns at B and C each had one shot fail (points 164, feature edges 220): a worker
  landed late. Both reruns read max 0.
- W6 / W7 transition strips: 46 / 48 max 0. The 2 classic Smoke rows differ, as expected now that they dissolve. Forward = backward on all 48.
- W8 DXV / video pictures byte-identical to a W8 build (15 files). W4 scrub DXV 1080 + 4K: 151 / 151.
- Deform renders vs W8: max 0, except Noise Warp max 1 / 255 (its phase became a global for the slit history).
- W14 wrap test: every new clock ok. Thermal's shimmer first read SPIKE (ratio 2.76); fixed with 64-periodic noise, now 0.79. The Lines clocks
  run flat over 180 min.

## Arena clips (all untriggered, opacity 1, selection parked on L18 C8)
- **Layer 14 "LoadEhr W9 A Lines"** (LoadEhr): C1 Plexus, C2 Light Paint, C3 Neon Tubes Text, C4 Plexus Audio, C5 Lines Too Much.
- **Layer 15 "LoadEhr W9 B Build"** (LoadEhr): C1 Slit-Scan, C2 Print On (Build Speed 4, faint wire), C3 Exploded Text (On Beat 2), C4 Black Hole,
  C5 Build Too Much.
- **Layer 16 "LoadEhr W9 C Cut"**: C1 Cross-Section, C2 Reveal Box, C3 Window Slot B, C6 Cut Too Much (LoadEhr); C4 Window Video, C5 Cross-Section
  Smoke (LoadEhr Wow).
- **Layer 17 "LoadEhr W9 D Surface"** (LoadEhr): C1 Audio Spikes, C2 Skin Of Copies, C3 X-Ray, C4 Thermal, C5 Night Vision, C6 LED Matrix,
  C7 Surface Too Much.
- **Layer 18 "LoadEhr W9 E Shape"** (LoadEhr): C1 Cube, C2 Torus, C3 Crystal Text, C4 Sphere, C5 Shape Too Much.
- All are soft (Spin Y 3, Key 0.8). Dialed by `tools/arena_w9.py <stage> <layer>`. Earlier layers were not touched.

## Deviations
- **Black Hole** is in Stage B (a Build Look), as the brief allowed.
- **Slit-scan history** has no letter states (pose, deform phases and jelly only).
- **Skin Of Copies** uses one LOD per batch, not per copy.
- **Neon light** is the glow-buffer approximation.
- **Spikes / copies / window** follow the dominant model only mid-hop.
- **Window world source:** in the Wow bundles it is the projector or the first other filled slot (there is no Surf Source there).
- **Window on models without eyes or glass** uses the silhouette.
- **Window runs the eye finder,** which also auto-places Light C's positions once per model, as C Enable does.
- **Arena numbers** are PERF lines of the selected clip, not a null-armed GPU trace.
- **Prewarm** covers capture, lines, cap, x-ray, spikes and copies, but not the light-paint ribbon program.
- **Print On first use:** in Arena its first read showed one 53 ms frame (the second read was 85 fps / 1.03 ms). A first-use compile is probably
  left over.

## Open / needs Amrit's eyes
- Play the 28 clips and prune: "add, then prune".
- Is the Smoke Sim move (classic bundles lose sim smoke) acceptable, or should a pruned group go instead?
- The composition was not saved.

## Commons
New findings, synced and pushed (93120ad):
- LOADEHR-018: Apple GL has no glCopyBufferSubData.
- LOADEHR-019: capture twins via transform feedback.
- LOADEHR-020: DEPTH24_STENCIL8 and depth blits.
- LOADEHR-021: a clock read through noise must tile at its wrap.

## RECEIPTS
```
$ git log -1 --format="%H %d"; git status -sb | head -1   (code, before this handoff commit)
1d2dde3237422d0135caa357d5d519f432ae7d19  (HEAD -> main, origin/main)
## main...origin/main
$ git log --oneline 981fdd5..HEAD
1d2dde3 W9: tools/arena_w9.py (the 28 W9 clips, layers 14-18)
791f14c LoadEhr W9 Stage E: Shape Target (Spherize toward sphere / cube / torus / crystal)
d8be013 LoadEhr W9 Stage D: Surface Look (audio spikes, skin of copies, x-ray, thermal, night vision, LED matrix)
614d40f LoadEhr W9 Stage C: Cut looks in every bundle (cross-section + slab caps, reveal box, window into another slot / the projector)
81ef24f LoadEhr W9 Stage B: Build looks (slit-scan, print on + stencil cap, exploded, black hole + lens) in the DEFORM twins
2a8e8eb LoadEhr W9 Stage A: Lines (plexus, light paint, neon tubes) via CAPTURE twins; Smoke Sim -> Wow, Face -> LoadEhr FX
$ tools/regress.sh w9/e_regress (final)
ref-hash guard ok: 6 references match tools/regress_refs.sha256
compare verify/w1/ref/r1_mask_front.png vs verify/w9/e_regress/r1_mask_front.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r2_wireframe50.png vs verify/w9/e_regress/r2_wireframe50.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r3_explode.png vs verify/w9/e_regress/r3_explode.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r4_fx_skin_env_input.png vs verify/w9/e_regress/r4_fx_skin_env_input.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r5_chain_mid_hop_b_c.png vs verify/w9/e_regress/r5_chain_mid_hop_b_c.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r6_chain_rest_d.png vs verify/w9/e_regress/r6_chain_rest_d.png: mean 0.0000 max 0 pixels>2 0 of 2073600
G1 ok   LoadEhr: 494 params, none named Library* / Laser*
G6 ok   LoadEhr: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhr: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G1 ok   LoadEhrFX: 507 params, none named Library* / Laser*
G6 ok   LoadEhrFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G1 ok   LoadEhrWow: 504 params, none named Library* / Laser*
G6 ok   LoadEhrWow: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWow: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G1 ok   LoadEhrWowFX: 509 params, none named Library* / Laser*
G6 ok   LoadEhrWowFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWowFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G2 ok   the only directory API: ModelLoader.cpp FolderIndex (textures next to the chosen model file):
G3 ok   no model / texture file inside build/LoadEhr*.bundle (8 files: binaries + Info.plist)
G5 ok   no Laser* / Library* sources, no library tools
G6 ok   no Mirror* source, no MIRROR program variant
G7 ok   no libtess2 / tesselator / SGI-B in src, third_party, CMakeLists.txt
G7 ok   no LGPL file in the tree (1144 tracked files, submodules included)
$ tools/w9_same.py verify/w9/e_same_all wire points strips scrub dxv <W8 build>
| check | compared | worst | differing |
|---|---|---|---|
| W5 wire renders | 48 PNGs | worst max 0 | all max 0 |
| W6 points renders | 28 PNGs | worst max 0 | all max 0 |
| W6 / W7 transition strips | 48 PNGs | worst max 255 | skull_helmet_18_smoke.png max 250, text_skull_18_smoke.png max 255 |
| strips forward = backward | 48 rows | worst 0 | |
| W4 scrub sweep (DXV) | 2 | | code_dxv_1080 151/151 yes, 0 wrong; code_dxv_4k 151/151 yes, 0 wrong |
| W8 DXV / video pictures vs w8build | | | | resolume-alley_dxt1_testsrc2_1920x1080.mov | 3 | 3 | 0 | 0.0000 | | resolume-alley_dxt5_testsrc2_1920x1080.mov | 3 
$ tools/w9_shape.py same (Sphere vs W8 Spherize)
| model | Spherize | max diff vs w8build |
|---|---|---|
| skull | 0.3 | 0 |
| skull | 0.7 | 0 |
| skull | 1 | 0 |
| text | 0.3 | 0 |
| text | 0.7 | 0 |
| text | 1 | 0 |
$ pgrep Arena (before -> after the restart): 27709 -> 15450
$ ls -la ~/Documents/Resolume\ Arena/Extra\ Effects | grep LoadEhr
drwxr-xr-x@  3 <user>  staff     96 Oct  7 20:05 LoadEhr.bundle
drwxr-xr-x@  3 <user>  staff     96 Oct  7 20:05 LoadEhrFX.bundle
drwxr-xr-x@  3 <user>  staff     96 Oct  7 20:05 LoadEhrWow.bundle
drwxr-xr-x@  3 <user>  staff     96 Oct  7 20:05 LoadEhrWowFX.bundle
$ ls -la verify/w9 (sizes, B)
288 a_combos;160 a_cost;462352 a_montage.png;320 a_regress;992 a_renders;448 a_same;160 a_same_dxv;1381118 a_strip_montage.png;800 a_strips;288 a_times;256 a_wraps;1664 arena_perf.md;1211728 b_combo_montage.png;416 b_combos;160 b_cost;1088 b_deform_same;586701 b_montage.png;320 b_regress;1248 b_renders;320 b_same;160 b_same2;1344 b_strips;224 b_wraps;416 c_combos;160 c_cost;1059551 c_montage.png;320 c_regress;992 c_renders;320 c_same;160 c_same2;672 c_strips;224 c_wraps;480 d_combos;160 d_cost;1956566 d_montage.png;320 d_regress;1760 d_renders;320 d_same;1024 d_strips;224 d_wraps;160 d_wraps2;160 e_cost;910583 e_montage.png;320 e_regress;1248 e_renders;672 e_same;448 e_same_all;1568 e_strips;
$ verify/w9/arena_perf.md (MBP M3 Max, Wire closed, 120 Hz, the selected clip, not triggered)
| clip (selected, not triggered) | fps delivered (median) | cpu ms | render ms | PERF lines |
|---|---|---|---|---|
| L14 C1 W9 Plexus | 94.6 | 0.92 | 0.95 | 3 |
| L14 C2 W9 Light Paint | 91.9 | 1.12 | 1.15 | 4 |
| L14 C3 W9 Neon Tubes Text | 81.0 | 1.55 | 1.59 | 4 (2nd read) |
| L14 C4 W9 Plexus Audio | 94.1 | 0.98 | 0.97 | 4 |
| L14 C5 W9 Lines Too Much | 91.4 | 1.25 | 1.26 | 4 |
| L15 C1 W9 Slit-Scan | 100.8 | 0.64 | 0.63 | 4 |
| L15 C2 W9 Print On | 85.1 | 0.96 | 1.03 | 4 (2nd read; the 1st, 1 line, caught a 53 ms first-use frame) |
| L15 C3 W9 Exploded Text | 88.1 | 0.71 | 0.69 | 4 (2nd read) |
| L15 C4 W9 Black Hole | 91.8 | 0.96 | 1.00 | 4 |
| L15 C5 W9 Build Too Much | 96.1 | 0.66 | 0.67 | 4 |
| L16 C1 W9 Cross-Section | 97.6 | 0.67 | 0.70 | 4 |
| L16 C2 W9 Reveal Box | 96.6 | 0.77 | 0.83 | 4 |
| L16 C3 W9 Window Slot B | 97.9 | 0.64 | 0.67 | 4 |
| L16 C4 W9 Window Video | 90.6 | 0.96 | 0.98 | 5 |
| L16 C5 W9 Cross-Section Smoke | 86.8 | 2.03 | 1.80 | 4 |
| L16 C6 W9 Cut Too Much | 94.6 | 0.87 | 0.87 | 4 |
| L17 C1 W9 Audio Spikes | 86.6 | 1.07 | 1.07 | 4 |
| L17 C2 W9 Skin Of Copies | 91.2 | 1.12 | 1.15 | 4 |
| L17 C3 W9 X-Ray | 89.5 | 0.68 | 0.66 | 4 |
| L17 C4 W9 Thermal | 95.2 | 0.65 | 0.66 | 4 |
| L17 C5 W9 Night Vision | 95.9 | 0.62 | 0.64 | 4 |
| L17 C6 W9 LED Matrix | 97.1 | 0.61 | 0.63 | 4 |
| L17 C7 W9 Surface Too Much | 92.1 | 0.95 | 0.94 | 4 |
| L18 C1 W9 Shape Cube | 101.4 | 0.59 | 0.58 | 4 |
| L18 C2 W9 Shape Torus | 96.9 | 0.59 | 0.62 | 4 |
| L18 C3 W9 Shape Crystal Text | 84.2 | 0.75 | 0.74 | 4 (2nd read) |
| L18 C4 W9 Shape Sphere | 97.9 | 0.59 | 0.63 | 4 |
| L18 C5 W9 Shape Too Much | 100.9 | 0.56 | 0.58 | 4 |
```
END OF HANDOFF w9-looks-pack-results sha256:78b701a7 19711
