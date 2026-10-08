project:    LoadEhr
pass:       w6-points-transitions
STATUS:     COMPLETE
parent:     f70fd37
result:     945d6c8 f330588 9c3bcf2
session:    0418ab55-f819-4194-9d0e-250000f60225
written_at: 2026-10-08T18:26:02Z
# LoadEhr W6 results: points only, and 18 new model-to-model transition styles

## Amrit's answers (recorded from the brief, 2026-10-08, verbatim)
1. Where did ~/Movies/KLV Set go? "Ran out of space, so deleted it." Intended. The W3 clips pointing into it were left alone; the W3
   proof now skips the two missing files and lists them.
2. Should Wire Only default to full strength? "At full."
3. Prune Scrub to DXV / HAP? "Limited."
4. This session: Amrit restarted Arena when asked (PID 43403 -> 2462). No other questions were asked.

## What shipped (commits 945d6c8, f330588, 9c3bcf2; pushed. Commons 64c368e: LOADEHR-011, -012, -013)
- **Stage 0, his answers.**
  - "At full": Wire Mode now defaults to Off and Wireframe Mix to 1. A fresh clip shows no wire; any mode he picks draws at full
    strength. Old clips: a W5 clip keeps its saved mode and mix. A pre-W5 clip that used the W1 wire (Wireframe Mix > 0) now opens with
    Wire Mode Off, so its wire is gone until Wire Mode is set to Over Surface.
  - "Limited": Video Loop Scrub runs only on DXV and HAP (all variants). On H.264, HEVC, ProRes and Motion JPEG, Scrub plays as Loop and
    Video Time starts "Scrub: DXV / HAP only". Cue and reverse speed still work for every codec.
- **Stage 1, points** ("Can we have points only also?").
  - Wire Mode gains Points Only and Points + Wire.
  - New dials: Point Source (Vertices / Feature Vertices / Surface 25k / 100k / 400k, default 100k) and Point Twinkle.
  - Every point rides its triangle through deforms, letters, shatter / pieces, clones and the morph pair.
  - The points use the wire look: Wire Color, Glow, Bloom, Fade, Back, and Wire Thickness for size.
  - Params: LoadEhr 503, FX 507, Wow 497, Wow FX 511 of 512. Nothing was moved or cut.
- **Stage 2, the transition pack** ("how many transitions ... can be added?": no hard limit; one dropdown, no param room used).
  - Transition Style gains styles 5..22. Styles 0..4 keep their numbers, so saved clips keep their style.
  - Style Order Cycle / Random go round all 23 styles. While Transition Play loops, every pass moves the style on, so a 4-slot loop
    meets every style. (Before that fix, Cycle in a 4-slot loop only ever showed 4 styles.)
  - Style table. Ease shapes every style's ramps. Costs are harness 1080p GPU ms mid-hop, skull -> helmet; rest 0.82, Dissolve 1.22.

| # | style | bundles | Stagger | Spread | GPU ms |
|---|---|---|---|---|---|
| 5 | Points (cloud A -> B) | all | departure order (bottom ring first) | arc height | 1.18 |
| 6 | Shrinkwrap | all | top first | - | 0.78 |
| 7 | Wire | all | - | - | 2.66 |
| 8 | Voxel | all | - | cube size | 1.04 |
| 9 | Low Poly | all | - | facet size | 1.10 |
| 10 | Pieces (to ranked partner pieces) | all | bottom first | arc | 0.89 |
| 11 | Implode | all | rim first | swirl | 0.34 |
| 12 | Twist | all | - | twist amount | 1.75 |
| 13 | Melt | all | - | - | 1.59 |
| 14 | Scan | all | - | wire band width | 0.79 |
| 15 | Glitch (slider-stepped, 30 steps / hop) | all | - | jump size | 0.89 |
| 16 | Ripple | all | - | wave height | 0.79 |
| 17 | Swarm | Wow (classic: Dissolve) | departure order | loop size | 0.89 |
| 18 | Smoke (sim) | all (the sim is in every bundle) | - | - | 2.07 |
| 19 | Burn | Wow (classic: Dissolve) | - | front width | 2.42 |
| 20 | Lava | Wow (classic: Dissolve) | - | - | 2.48 |
| 21 | Spin Swap | all | - | turns | 0.84 |
| 22 | Fold | all | - | X / X+Y / X+Y+Z | 3.06 |

## Validated
- **End-pop proof** (`tools/w6_trans.py strips`): 18 styles x 3 pairs (skull -> SciFi helmet and text -> skull in LoadEhr; skull ->
  Crown 1918 in Wow), 9-step strips run forward and then backward.
  - Forward vs backward: max 0 at every step (bit-identical).
  - t = 0 and t = 1 vs the plain models: max 0.
  - Onsets at t = 0.01 / 0.99: mean <= 0.024, max <= 75 (DEFORM-twin normals at the first twist / melt).
  - Smoke's strip is its mesh alone; its smoke is a live sim and is checked separately when played.
- **D -> A hops** (4 slots + Chain Loop): every style works mid-hop (verify/w6/s2_chain/chain_da.png).
- **Points** (`tools/w6_points.py`):
  - Point counts on the skull: Vertices 15,572; Feature 6,550; 25k / 100k / 400k exact.
  - Coverage at 25k / 100k / 400k: 0.073 / 0.164 / 0.218.
  - Shatter: the points spread like the wire at the same flight time.
  - Twinkle: per-frame change 0 -> 0.49 when switched on; its 1000-twinkle wrap is ok.
  - Harness GPU ms (skull): solid 0.77, Vertices 1.68, 100k 2.35, 400k 4.97.
  - Arena 7.28, MBP M3 Max, Wire closed, nothing playing, one selected clip: solid and every point source deliver 121-125 fps. That is
    the 120 Hz cap, so it is a floor. Plugin CPU goes from 0.13-0.17 ms to 0.21-0.25 ms. An earlier run in Arena's ~85 fps regime
    showed no difference within the spread either.
- **Stage 0**:
  - Scrub on ProRes / H.264 / HEVC is refused and plays as Loop; DXV / HAP scrub.
  - W4 sweep: DXV / HAP / HAP Q at 1080 + 4K pass. Cue: H.264, HEVC, DXV pass. Reverse: H.264, H.264 g30, DXV pass.
- **No frame waits on a load**: until a style's per-slot data lands, the hop dissolves.
  - 4-slot Cycle loop, 1280x720 harness, cold: worst frame 35 ms in LoadEhr and 49 ms in Wow. These are each model's first-draw
    shader prewarm.
  - Second 12 s: p99 9.6 / 13.3 ms.
- **Clocks**: every w14 wrap row ok (+ the Point Twinkle row). The 180 min chain (Shrinkwrap, Fixed) shows the same per-frame change at
  1 / 13 / 26 / 60 / 180 min (p95 2.92).
- **Other checks**: regress max 0 (6 references); guards G1-G7 ok on all four bundles; the 48 W5 wire renders identical (max 0); W3
  DXV proof unchanged on the 11 remaining files (max 1-4).

## Deviations / found on the way
- **Not removed**: the Scrub "nearest decoded frame on the way" fallback. It was not inert: it fired on HAP / HAP Q 4K sweeps (25 / 63
  pictures on the way). Per the dead-code rule it stays. The backward-read start rule serves a cue's hold too, so it also stays.
- **Bug fixed (LOADEHR-011)**: an Apple GL miscompile. A mid-shader discard (the dissolve, the alpha cutout) corrupted later values in
  the WIRE twins, so incoming models came out white mid-hop. This also affected W5's Dissolve + wire. The WIRE twins now cut through
  the end-of-shader alpha test; the plain programs are untouched.
- **text -> logo is not possible**: a logo can only live in slot A (Model Source), so a text -> logo chain does not exist. Text -> skull
  stands in for it.
- **Smoke breaks the "still is still" rule for its smoke only**: the mesh is slider-driven, but the smoke is a sim. It is emitted in
  proportion to how fast the hop moves, and it lingers for a 4 s lit tail.
- **Not morphed**: the cloud and the mirror floor ignore deforms and clones. Letters are not applied during MORPH-program hops.
- **InitGL per new instance went from 1.02 s (W5) to 1.63 s**, because more program twins (points, morph) are linked up front. This
  is for Amrit's eyes; see question 1.

## Open, for Amrit's eyes
1. Is the longer plugin start (+0.6 s when a LoadEhr clip instance is created) acceptable, or should the point / morph programs be
   linked lazily? That would trade it for a one-time hitch the first time each feature is used.
2. Prune: after playing the transition clips, which of the 23 styles stay?

## Arena clips (untriggered; selection parked on L10 C9)
- **Layer 10 "LoadEhr W6 Points"** (LoadEhr Wow, opacity 1):
  - C1 Vertices, C2 Feature, C3 25k, C4 100k, C5 400k: the skull, cyan, glow 2.
  - C6 Points + Wire: helmet, amber, Feature Vertices + Feature Edges.
  - C7 Text Twinkle: "LOADEHR", Point Twinkle 0.8.
  - C8 Points Shatter: skull, Shatter On Beat Every 4.
- **Layer 11 "LoadEhr W6 Transitions"** (LoadEhr Wow, opacity 1; Ping-Pong, 6 s per hop, Hold 1.5 s, two different models each):
  - C1 Points, C2 Shrinkwrap, C3 Wire, C4 Voxel, C5 Low Poly, C6 Pieces, C7 Implode, C8 Twist, C9 Melt, C10 Scan, C11 Glitch,
    C12 Ripple, C13 Swarm, C14 Smoke, C15 Burn, C16 Lava, C17 Spin Swap, C18 Fold.
  - C19 Cycle All Styles: 4 slots (skull, helmet, crown, Nefertiti), Chain Loop, Style Order Cycle.
- Dials: `tools/arena_w6.py points 10` / `trans 11`. Earlier clips were not touched. No model path contains "carnival".

## RECEIPTS
```
$ git log -1 ; git status -sb ; git log --oneline f70fd37..HEAD
9c3bcf226613011256cdffe00c28b5076a0fbca5 LoadEhr W6 Stage 2: the transition pack (Transition Style 5..22), every bundle
## main...origin/main
9c3bcf2 LoadEhr W6 Stage 2: the transition pack (Transition Style 5..22), every bundle
f330588 LoadEhr W6 Stage 1: Points Only / Points + Wire, Point Source, Point Twinkle (every bundle)
945d6c8 LoadEhr W6 Stage 0: Wire Mode Off + Wireframe Mix 1 by default; Scrub only on DXV / HAP

$ tools/regress.sh w6/final2_regress   (final build)
compare verify/w1/ref/r1_mask_front.png vs verify/w6/final2_regress/r1_mask_front.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r2_wireframe50.png vs verify/w6/final2_regress/r2_wireframe50.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r3_explode.png vs verify/w6/final2_regress/r3_explode.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r4_fx_skin_env_input.png vs verify/w6/final2_regress/r4_fx_skin_env_input.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r5_chain_mid_hop_b_c.png vs verify/w6/final2_regress/r5_chain_mid_hop_b_c.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r6_chain_rest_d.png vs verify/w6/final2_regress/r6_chain_rest_d.png: mean 0.0000 max 0 pixels>2 0 of 2073600
G1 ok   LoadEhr: 503 params, none named Library* / Laser*
G1 ok   LoadEhrFX: 507 params, none named Library* / Laser*
G1 ok   LoadEhrWow: 497 params, none named Library* / Laser*
G1 ok   LoadEhrWowFX: 511 params, none named Library* / Laser*
G4 ok   (each bundle) covered pixels 0 ; 4 empty slots; Status A = 'Nothing loaded: transparent output. ...'
G2 ok   the only directory API: ModelLoader.cpp FolderIndex
G3 ok   no model / texture file inside build/LoadEhr*.bundle (8 files: binaries + Info.plist)
G5 ok   no Laser* / Library* sources, no library tools
G6 ok   no Mirror* source, no MIRROR program variant
G7 ok   no libtess2 / tesselator / SGI-B in src, third_party, CMakeLists.txt
G7 ok   no LGPL file in the tree (623 tracked files, submodules included)

$ tools/w6_points.py wire_same verify/w6/final_wire_same
worst: 0 over 48 shots

$ tools/w14_wraps.py times verify/w6/final2_times_fixed chainfixed
chainfixed      1 |     0.000     2.917    33.811
chainfixed     13 |     0.001     2.916    33.817
chainfixed     26 |     0.001     2.917    33.825
chainfixed     60 |     0.001     2.921    33.840
chainfixed    180 |     0.001     2.924    33.709

$ tools/w6_trans.py waits verify/w6/final_waits
LoadEhr: stats COLD  frames 641 | plugin wall ms avg 4.85 p99 12.13 max 34.62 | GPU ms avg 3.13 p99 9.18 max 24.82
LoadEhr: stats WARM  frames 641 | plugin wall ms avg 4.81 p99  9.63 max 37.82 | GPU ms avg 3.25 p99 7.65 max 36.14
LoadEhrWow: stats COLD frames 634 | plugin wall ms avg 5.45 p99 29.49 max 48.65 | GPU ms avg 3.44 p99 19.34 max 46.28
LoadEhrWow: stats WARM frames 642 | plugin wall ms avg 5.63 p99 13.28 max 43.76 | GPU ms avg 3.36 p99  9.21 max 41.66

$ ls -la ~/Documents/Resolume\ Arena/Extra\ Effects/LoadEhr*.bundle/Contents/MacOS/
-rwxr-xr-x  1 amritrosell  staff  4588016 Oct  8 20:13 LoadEhr
-rwxr-xr-x  1 amritrosell  staff  4660400 Oct  8 20:13 LoadEhrFX
-rwxr-xr-x  1 amritrosell  staff  4588016 Oct  8 20:13 LoadEhrWow
-rwxr-xr-x  1 amritrosell  staff  4660400 Oct  8 20:13 LoadEhrWowFX

$ ls -la verify/w6   (evidence, local; selection)
drwxr-xr-x  60 ... 1920 Oct  8 19:50 s2_strips
drwxr-xr-x  22 ...  704 Oct  8 19:50 s2_chain
drwxr-xr-x   4 ...  128 Oct  8 19:59 s2_cost
drwxr-xr-x  15 ...  480 Oct  8 19:57 s2_smoke
drwxr-xr-x  35 ... 1120 Oct  8 20:00 final_points
drwxr-xr-x  22 ...  704 Oct  8 19:31 s1_follow
drwxr-xr-x  15 ...  480 Oct  8 19:20 s0_refuse
drwxr-xr-x  15 ...  480 Oct  8 19:24 s0_sweep

Arena (REST): PID 2462 (was 43403); connected clips 0; selection parked on L10 C9
LoadEhr W6 Points: 8 clips: W6 Points Vertices, Feature, 25k, 100k, 400k, Points + Wire, Points Text Twinkle, Points Shatter
LoadEhr W6 Transitions: 19 clips: W6 Points .. W6 Fold (18 styles) + W6 Cycle All Styles
Arena PERF (selected clip, M3 Max, Wire closed, 1920x1080): solid skull 121.7-124.2 fps / cpu 0.13-0.17 ms;
Points 100k / 400k / Vertices 121.5-124.6 fps / cpu 0.21-0.25 ms (120 Hz cap: a floor)
```
END OF HANDOFF w6-points-transitions sha256:39f172e3 12518
