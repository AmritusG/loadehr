project:    LoadEhr
pass:       w12-lightc-glide
STATUS:     CHECKPOINT
parent:     c621216
result:     7b757c0
session:    7fb29fc7-1678-4bc4-ba23-11a2293a5935
written_at: 2026-10-11T10:41:47Z

# LoadEhr W12: Light C "Every Model Swap" glides cone and depth (CHECKPOINT: code done + verified; Arena steps PARKED)

## Your words (verbatim, 2026-10-11)
"Lights can glide, looks more natural." (the answer to: with C Auto Place = Every Model Swap the cone / Behind Depth re-fit jumps in
one step at the hop's midpoint; glide or keep the step?)

## What shipped (7b757c0, pushed)
- Every Model Swap: the cone and Behind Depth the lights use now glide from the outgoing model's fit to the incoming one's on the
  openings' own glide (smoothstep t 0.35 .. 0.65), so position, cone and depth arrive together. The dials still change at the hop's
  midpoint (the placement writes them, as in W11), and the glide's end is exactly what gets written: a model not yet placed is
  predicted with the placement's own formula from the current dials.
- Each model keeps its own fit (the dials while it was on show, hand edits included); coming back to a model writes that fit back.
- A jump of the chain (a new hop before arrival, a restart, a fast drag) glides on from what was on screen: a carry on cone, depth
  and the openings that fades over 0.3 x Transition Time (the glide's share of a hop). No snap.
- The C Place At Eyes / C Fit Cone buttons still write the dials at once. Aim / Anchor / Mirror X are on/off switches: no glide.
- Off / Once Per Model / Position Only: untouched (the lights read the dials as before; renders max 0 vs W11).
- tools/w12_glide.py (hop / interrupt / same), CLAUDE.md W12 section, docs/audit + param_hierarchy.md re-generated.

## Deviation (needs your eyes)
- **Coming back to a model restores its own fit.** W11 re-fitted from the current dials, and its rule only ever LOWERS Behind Depth
  (0.6 x the back wall), so skull -> helmet -> skull came back at the helmet's 0.035 instead of the skull's 0.157. Gliding both ways
  without a step needs the end values to be known ahead, so a revisit now writes back the model's own fit. First arrival on each
  model: the same values as W11's one-step fit (readback below). If you want W11's ratchet back, say so.
- Also tested skull -> Goat Skull: on skull -> Damaged Helmet the cone sits at its 40 deg cap on both models, so only depth and
  position move; the Goat Skull's see-through eyes fit 24 deg and show the cone glide too.

## PARKED (CONTRACT 11.3): install, Arena restart, clips
- `_Commons/ARENA-HELD-BY.txt` = "Fanous W3 (session 2026-10-11T11:50 — Arena-only cost diagnosis: REST reads, Metal traces, GPU
  measurement)". Held by another session, so not waited for and not taken. NOT installed; no Arena restart; no clip built.
- **Fanous should know:** my offline harness ran ~12:00-12:40 inside their lock window (W11 reference build, the glide tests, a Light C
  param sweep on four bundles). CONTRACT 11.6 says check the lock before a long offline run; I checked it only at the Arena step and
  then stopped. Their GPU traces from that window may carry a LoadEhr harness process.
- To resume when the lock is free: (1) take the lock; (2) install the four bundles (CLAUDE.md loop) from build/ at 7b757c0;
  (3) you cmd-Q and relaunch Arena, check the PID changed (it was 3910); (4) NEW layer "LoadEhr W12 Light Glide", untriggered: C1 =
  LoadEhr, Model A Human Skull, Model B test_models/DamagedHelmet/DamagedHelmet.glb, models shown,
  Haze 0.25, Smoke Amount 0, Orbit Yaw 60, C Auto Place Every Model Swap, preset C Aim Mode At Camera / C Cone Angle 70 /
  C Behind Depth 1.5 / C Anchor To Mask off, then C Enable 1, Transition Play Ping-Pong, Transition Time 6, Hold Time 1.5;
  C2 = the same with C Auto Place Once Per Model. (5) remove the lock.

## Validated (the numbers per claim)
- No spike at the midpoint (beam only, slider 0 -> 1 -> 0 in 100 steps each way, 8 frames a step, mean change per step):
  skull -> Damaged Helmet max step W12 Every 0.284 vs W11 Every 0.861 (at t 0.50), Once 0.294; skull -> Goat Skull W12 Every 0.219 vs
  W11 Every 5.00, Once 0.383.
- Interrupted hop (skull -> Goat -> Damaged, slider 0.35 -> 0.8 in one frame): max per frame W12 Every 0.138 vs W11 Every 6.54
  (Once 7.65: Once's openings jump too, untouched by design).
- Readback on arrival: W12 = W11 (Behind 0.0350175, cone 40, positions -0.510856 / 0.266226 / 0.465788).
- Other modes: Off / Once / Position, skull -> Goat at 5 points of the hop: max 0 vs W11 in all 15, repeat floor max 0, each mode's light
  moves up to 730 px. Run without haze because the volume march is not run-to-run exact (first run of a series is a cold outlier; with 150 settle
  frames, runs 2-4 of Once were bit-identical and max 0 vs W11, but a 4-run series over 3 modes varied up to max 66).
- Regress 6/6 max 0; guards G1-G7 ok; param audit DEAD 0 / BROKEN 0 / MISROUTED 0, canuse 0; counts 487 / 508 / 497 / 510 (unchanged).
- Also done: W11b (Commons LOADEHR-025, c621216), the prerequisite this brief named.

## RECEIPTS
```
$ git log -1 ; git status -sb
7b757c03c458d842a4c1be060b4363a1785253b3 LoadEhr W12: Light C cone / Behind Depth glide with C Auto Place Every Model Swap
## main...origin/main
$ tools/regress.sh w12/regress   (verify/w1/ref vs this build)
compare verify/w1/ref/r1_mask_front.png vs r1_mask_front.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r2_wireframe50.png vs r2_wireframe50.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r3_explode.png vs r3_explode.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r4_fx_skin_env_input.png vs r4_fx_skin_env_input.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r5_chain_mid_hop_b_c.png vs r5_chain_mid_hop_b_c.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r6_chain_rest_d.png vs r6_chain_rest_d.png: mean 0.0000 max 0 pixels>2 0 of 2073600
$ tools/guards.sh
G1 ok   LoadEhr: 487 params, none named Library* / Laser*
G6 ok   LoadEhr: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhr: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G1 ok   LoadEhrFX: 508 params, none named Library* / Laser*
G6 ok   LoadEhrFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G1 ok   LoadEhrWow: 497 params, none named Library* / Laser*
G6 ok   LoadEhrWow: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWow: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G1 ok   LoadEhrWowFX: 510 params, none named Library* / Laser*
G6 ok   LoadEhrWowFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWowFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G2 ok   the only directory API: ModelLoader.cpp FolderIndex (textures next to the chosen model file):
G3 ok   no model / texture file inside build/LoadEhr*.bundle (8 files: binaries + Info.plist)
G5 ok   no Laser* / Library* sources, no library tools
G6 ok   no Mirror* source, no MIRROR program variant
G7 ok   no libtess2 / tesselator / SGI-B in src, third_party, CMakeLists.txt
G7 ok   no LGPL file in the tree (1045 tracked files, submodules included)
$ tools/param_audit.py static ; sweep <4 bundles> "Light C" --fresh ; map ; canuse
static: 708 ids, carried {'LoadEhr': 487, 'LoadEhrFX': 508, 'LoadEhrWow': 497, 'LoadEhrWowFX': 510}; not read / misrouted: 0
DEAD 0
BROKEN 0
MISROUTED 0
canuse: 66 offered rows checked, 2 kept by Amrit, CANNOT USE left: 0
$ tools/w12_glide.py hop verify/w12/hop <W11 build> damaged
| build | C Auto Place | direction | max step (at t) | step at the swap (t 0.50-0.51) | mean step t 0.35-0.65 | mean step elsewhere |
|---|---|---|---|---|---|---|
| W12 | Every | fwd | 0.284 (0.52) | 0.273 | 0.187 | 0.045 |
| W12 | Every | back | 0.284 (0.51) | 0.279 | 0.188 | 0.046 |
| W12 | Once | fwd | 0.294 (0.51) | 0.294 | 0.194 | 0.043 |
| W12 | Once | back | 0.292 (0.51) | 0.289 | 0.194 | 0.040 |
| W11 | Every | fwd | 0.861 (0.50) | 0.861 | 0.204 | 0.041 |
| W11 | Every | back | 0.269 (0.53) | 0.269 | 0.180 | 0.037 |
| W11 | Once | fwd | 0.293 (0.52) | 0.287 | 0.193 | 0.039 |
| W11 | Once | back | 0.292 (0.51) | 0.289 | 0.194 | 0.038 |

| build | C Auto Place | at | C Cone Angle | C Behind Depth | C Left Pos X | C Left Pos Y | C Left Pos Z |
|---|---|---|---|---|---|---|---|
| W12 | Every | rest on skull (t 0) | 40 | 0.157013 | -0.274343 | 0.111915 | 0.702294 |
| W12 | Every | arrived on B (t 1) | 40 | 0.0350175 | -0.510856 | 0.266226 | 0.465788 |
| W12 | Every | back on skull (t 0) | 40 | 0.157013 | -0.274343 | 0.111915 | 0.702294 |
| W12 | Once | rest on skull (t 0) | 40 | 0.157013 | -0.274343 | 0.111915 | 0.702294 |
| W12 | Once | arrived on B (t 1) | 40 | 0.157013 | -0.274343 | 0.111915 | 0.702294 |
| W12 | Once | back on skull (t 0) | 40 | 0.157013 | -0.274343 | 0.111915 | 0.702294 |
| W11 | Every | rest on skull (t 0) | 40 | 0.157013 | -0.274343 | 0.111915 | 0.702294 |
$ tools/w12_glide.py hop verify/w12/hop <W11 build> goat
| build | C Auto Place | direction | max step (at t) | step at the swap (t 0.50-0.51) | mean step t 0.35-0.65 | mean step elsewhere |
|---|---|---|---|---|---|---|
| W12 | Every | fwd | 0.219 (0.52) | 0.212 | 0.148 | 0.043 |
| W12 | Every | back | 0.219 (0.51) | 0.212 | 0.149 | 0.044 |
| W12 | Once | fwd | 0.382 (0.52) | 0.381 | 0.248 | 0.036 |
| W12 | Once | back | 0.383 (0.51) | 0.381 | 0.248 | 0.036 |
| W11 | Every | fwd | 4.998 (0.50) | 4.998 | 0.321 | 0.039 |
| W11 | Every | back | 5.031 (0.49) | 5.031 | 0.323 | 0.039 |
| W11 | Once | fwd | 0.384 (0.52) | 0.379 | 0.247 | 0.034 |
| W11 | Once | back | 0.383 (0.51) | 0.381 | 0.248 | 0.035 |

| build | C Auto Place | at | C Cone Angle | C Behind Depth | C Left Pos X | C Left Pos Y | C Left Pos Z |
|---|---|---|---|---|---|---|---|
| W12 | Every | rest on skull (t 0) | 40 | 0.157013 | -0.274343 | 0.111915 | 0.702294 |
| W12 | Every | arrived on B (t 1) | 24.2857 | 0.157013 | -0.282188 | 0.051246 | -0.0121589 |
| W12 | Every | back on skull (t 0) | 40 | 0.157013 | -0.274343 | 0.111915 | 0.702294 |
| W12 | Once | rest on skull (t 0) | 40 | 0.157013 | -0.274343 | 0.111915 | 0.702294 |
| W12 | Once | arrived on B (t 1) | 40 | 0.157013 | -0.274343 | 0.111915 | 0.702294 |
| W12 | Once | back on skull (t 0) | 40 | 0.157013 | -0.274343 | 0.111915 | 0.702294 |
| W11 | Every | rest on skull (t 0) | 40 | 0.157013 | -0.274343 | 0.111915 | 0.702294 |
$ tools/w12_glide.py interrupt verify/w12/int <W11 build>
| build | C Auto Place | frame 1 after the jump | max per frame (frame) | sum frames 1-150 | frames > 0.5 |
|---|---|---|---|---|---|
| W12 | Every | 0.138 | 0.138 (1) | 10.86 | 0 |
| W12 | Once | 0.092 | 7.648 (2) | 12.29 | 1 |
| W11 | Every | 0.142 | 6.539 (2) | 11.64 | 1 |
| W11 | Once | 0.083 | 7.647 (2) | 12.28 | 1 |
$ tools/w12_glide.py same verify/w12/same <W11 build>
| C Auto Place | t | W12 vs W11 | W12 vs W12 (repeat) | what the mode changes (vs Off, W12) |
|---|---|---|---|---|
| Off | 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | - |
| Off | 0.3 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | - |
| Off | 0.5 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | - |
| Off | 0.7 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | - |
| Off | 1 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | - |
| Once | 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.0001 max 8 pixels>2 4 |
| Once | 0.3 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.0001 max 8 pixels>2 4 |
| Once | 0.5 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.1775 max 117 pixels>2 651 |
| Once | 0.7 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.1438 max 160 pixels>2 516 |
| Once | 1 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.1462 max 160 pixels>2 488 |
| Position | 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.0020 max 30 pixels>2 31 |
| Position | 0.3 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.0021 max 30 pixels>2 33 |
| Position | 0.5 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.1677 max 143 pixels>2 730 |
| Position | 0.7 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.1371 max 154 pixels>2 528 |
| Position | 1 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.1402 max 155 pixels>2 500 |
$ ls -la verify/w12 ; du -sh   (proof output; cleared after this handoff, see below)
total 0
drwxr-xr-x   5 amritrosell  staff   160 Oct 11 12:26 .
drwxr-xr-x   5 amritrosell  staff   160 Oct 11 12:28 ..
drwxr-xr-x  36 amritrosell  staff  1152 Oct 11 12:21 hop
drwxr-xr-x  15 amritrosell  staff   480 Oct 11 12:20 int
drwxr-xr-x  68 amritrosell  staff  2176 Oct 11 12:27 same
360K	verify/w12/hop
116K	verify/w12/int
3.5M	verify/w12/same
3.4M	verify/verify/w12
$ ls -la build/*.bundle/Contents/MacOS/*   (built, NOT installed)
-rwxr-xr-x  1 amritrosell  staff  4824856 Oct 11 12:17 build/LoadEhr.bundle/Contents/MacOS/LoadEhr
-rwxr-xr-x  1 amritrosell  staff  4880952 Oct 11 12:17 build/LoadEhrFX.bundle/Contents/MacOS/LoadEhrFX
-rwxr-xr-x  1 amritrosell  staff  4808184 Oct 11 12:17 build/LoadEhrWow.bundle/Contents/MacOS/LoadEhrWow
-rwxr-xr-x  1 amritrosell  staff  4880816 Oct 11 12:17 build/LoadEhrWowFX.bundle/Contents/MacOS/LoadEhrWowFX
$ cat _Commons/ARENA-HELD-BY.txt ; pgrep Arena
Fanous W3 (session 2026-10-11T11:50 — Arena-only cost diagnosis: REST reads, Metal traces, GPU measurement); 3910 /Applications/Resolume Arena/Arena.app/Contents/MacOS/Arena
```
Proof output cleared after writing (verify/w12, verify/verify/w12: ~7 MB): the tables above are the record; tools/w12_glide.py rebuilds it.
END OF HANDOFF w12-lightc-glide sha256:7fd910f4 14003
