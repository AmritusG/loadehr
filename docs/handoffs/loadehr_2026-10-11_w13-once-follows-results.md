project:    LoadEhr
pass:       w13-once-follows
STATUS:     COMPLETE
parent:     efb233f
result:     d1dd3b4
session:    7fb29fc7-1678-4bc4-ba23-11a2293a5935
written_at: 2026-10-11T13:16:06Z

# LoadEhr W13: C Auto Place Once Per Model follows each model's depth / cone (COMPLETE)

## Your words (verbatim, 2026-10-11)
On W12's Arena clips: "C1 looks more correct, C2 just looks off, incorrect, flawed." Then, offered (1) fix Once Per Model itself or
(2) make Every Model Swap the default: "1".

## Why C2 looked wrong
Once Per Model moved each model's eye openings (W11 glide) but kept the anchor's cone and Behind Depth: on the Damaged Helmet the
skull's Behind Depth 0.157 put the lights behind the helmet's back wall (0.058 behind the opening), so the beams started inside the shell.

## What shipped (d1dd3b4, pushed)
- Once Per Model: the dials stay the anchor's (written once, hand edits kept, never overwritten on a hop). Every other model gets its
  own shift, like its openings: depth = min(dials, 0.6 x its back wall) (the placement's own rule), cone = dials + (its fit's cone - the
  anchor's fit's cone). It glides on the openings' glide, also while resting on a model, and the W12 carry covers jumps.
- Every Model Swap, Off, Position Only: unchanged.
- Found during it: my first cut showed the anchor's dials while resting at the end of an open chain (no next hop). Fixed and re-measured.
- tools/w13_once.py (hop / arrive / same), tools/arena_w13.py, CLAUDE.md W13 note.
- Arena: installed (installed vs build/, signatures stripped: 2-3 bytes differ, all inside the slices' Mach-O load-command headers,
  none in code), PID 97391 -> 71623 after your restart. NEW layer 23 "LoadEhr W13 Once Follows", untriggered: C1 (column 1) Once Per
  Model, C2 (column 2) Every Model Swap, both skull -> Goat Skull Ping-Pong (cone 40 -> 24 on the goat), the W12 clip setup otherwise.
  They should look the same. Layer 22's W12 C2 (Once, skull -> helmet) now shows the fix with its dials untouched. No other layer or
  clip touched, nothing playing, comp not saved by me. Lock taken (free) and removed.

## Validated (W13 = this build, W12 = 7b757c0's build)
- Resting on B after a hop, Once vs Every: damaged + haze mean 0.057 max 16 = exactly the Every-vs-Every repeat floor (W12: mean 10.42,
  max 134); goat + haze mean 0.166 max 18 (W12: 2.96, max 165); without haze both max 0 (W12 goat: 70 px, max 109).
- Hop, beam only, max step: Once skull -> damaged 0.284 / 0.278 (fwd / back), skull -> goat 0.229 / 0.211, level with Every (0.278 /
  0.230); W12 Once goat 0.386. Dials read back: Once never writes on a hop (positions / cone / depth = the skull's at every end).
- Interrupted hop (skull -> goat -> damaged, 0.35 -> 0.8): Once 0.142 per frame (W12 Once 7.65), Every 0.143.
- Off / Position Only / Every Model Swap: 15 renders max 0 vs W12, repeat floor max 0.
- Regress 6/6 max 0, guards G1-G7 ok, audit DEAD / BROKEN / MISROUTED 0, canuse 0, counts 487 / 508 / 497 / 510.

## Your eyes
- Trigger layer 23 C1 then C2 (should match), and layer 22 C2 again (should now look like C1 there).
- Still open from W12: on a revisit, Every Model Swap restores each model's own fit (W11 kept the lowered depth).

## RECEIPTS
```
$ git log -1 ; git status -sb
d1dd3b47037cbb44f98feaa05c87f5b1f0655eaf LoadEhr W13: C Auto Place Once Per Model follows each model's depth / cone
## main...origin/main
$ tools/regress.sh w13r (incl. guards)
compare verify/w1/ref/r1_mask_front.png vs r1_mask_front.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r2_wireframe50.png vs r2_wireframe50.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r3_explode.png vs r3_explode.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r4_fx_skin_env_input.png vs r4_fx_skin_env_input.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r5_chain_mid_hop_b_c.png vs r5_chain_mid_hop_b_c.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r6_chain_rest_d.png vs r6_chain_rest_d.png: mean 0.0000 max 0 pixels>2 0 of 2073600
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
G7 ok   no LGPL file in the tree (1048 tracked files, submodules included)
$ tools/param_audit.py static ; sweep x4 "Light C" --fresh ; map ; canuse
static: 708 ids, carried {'LoadEhr': 487, 'LoadEhrFX': 508, 'LoadEhrWow': 497, 'LoadEhrWowFX': 510}; not read / misrouted: 0
LoadEhrFX: 508 params swept -> docs/audit/sweep_LoadEhrFX.json
LoadEhr: 487 params swept -> docs/audit/sweep_LoadEhr.json
LoadEhrWow: 497 params swept -> docs/audit/sweep_LoadEhrWow.json
LoadEhrWowFX: 510 params swept -> docs/audit/sweep_LoadEhrWowFX.json
DEAD 0
BROKEN 0
MISROUTED 0
canuse: 66 offered rows checked, 2 kept by Amrit, CANNOT USE left: 0
$ tools/w13_once.py arrive verify/w13/arrive <W12 build>
| B | haze | Once vs Every, W13 | Once vs Every, W12 | Every vs Every (repeat, W13) |
|---|---|---|---|---|
| damaged | 0.25 | mean 0.0569 max 16 pixels>2 532 | mean 10.4197 max 134 pixels>2 20915 | mean 0.0569 max 16 pixels>2 532 |
| damaged | 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.0000 max 0 pixels>2 0 |
| goat | 0.25 | mean 0.1662 max 18 pixels>2 3433 | mean 2.9621 max 165 pixels>2 18395 | mean 0.0000 max 0 pixels>2 0 |
| goat | 0 | mean 0.0000 max 0 pixels>2 0 | mean 0.0187 max 109 pixels>2 70 | mean 0.0000 max 0 pixels>2 0 |
$ tools/w13_once.py hop ... damaged (W13 rows)
| W13 | Once | fwd | 0.284 (0.52) | 0.273 | 0.187 | 0.045 |
| W13 | Once | back | 0.278 (0.50) | 0.278 | 0.185 | 0.041 |
| W13 | Every | fwd | 0.278 (0.52) | 0.273 | 0.185 | 0.041 |
| W13 | Every | back | 0.281 (0.51) | 0.279 | 0.187 | 0.044 |
$ tools/w13_once.py hop ... goat (W13 / W12 Once rows)
| W13 | Once | fwd | 0.229 (0.52) | 0.212 | 0.151 | 0.049 |
| W13 | Once | back | 0.211 (0.52) | 0.211 | 0.147 | 0.038 |
| W12 | Once | fwd | 0.386 (0.52) | 0.379 | 0.249 | 0.037 |
| W12 | Once | back | 0.384 (0.51) | 0.380 | 0.248 | 0.034 |
$ tools/w12_glide.py interrupt (labels: this build / W12)
W13 Once 0.142 (frame 1) max 0.142, frames > 0.5: 0 | W13 Every 0.143 | W12 Once 7.647 (frame 2) | W12 Every 0.141
$ tools/w13_once.py same: 15 of 15 rows max 0 vs W12 and vs repeat
$ ls -la verify/w13 verify/w13r ; du -sh  (cleared after this handoff)
total 24
drwxr-xr-x   9 amritrosell  staff   288 Oct 11 14:56 .
drwxr-xr-x   5 amritrosell  staff   160 Oct 11 14:57 ..
drwxr-xr-x  77 amritrosell  staff  2464 Oct 11 14:56 arrive
-rw-r--r--   1 amritrosell  staff   571 Oct 11 14:56 arrive.log
drwxr-xr-x  20 amritrosell  staff   640 Oct 11 14:53 hop
-rw-r--r--   1 amritrosell  staff  1841 Oct 11 14:53 hop.log
drwxr-xr-x  15 amritrosell  staff   480 Oct 11 14:57 int
drwxr-xr-x  68 amritrosell  staff  2176 Oct 11 14:53 same
-rw-r--r--   1 amritrosell  staff  1238 Oct 11 14:53 same.log
6.0M	verify/w13
3.4M	verify/w13r
$ ls -la Extra Effects/LoadEhr*.bundle/Contents/MacOS/*
-rwxr-xr-x  1 amritrosell  staff  4879168 Oct 11 15:08 /Users/amritrosell/Documents/Resolume Arena/Extra Effects/LoadEhr.bundle/Contents/MacOS/LoadEhr
-rwxr-xr-x  1 amritrosell  staff  4935104 Oct 11 15:08 /Users/amritrosell/Documents/Resolume Arena/Extra Effects/LoadEhrFX.bundle/Contents/MacOS/LoadEhrFX
-rwxr-xr-x  1 amritrosell  staff  4862592 Oct 11 15:08 /Users/amritrosell/Documents/Resolume Arena/Extra Effects/LoadEhrWow.bundle/Contents/MacOS/LoadEhrWow
-rwxr-xr-x  1 amritrosell  staff  4934960 Oct 11 15:08 /Users/amritrosell/Documents/Resolume Arena/Extra Effects/LoadEhrWowFX.bundle/Contents/MacOS/LoadEhrWowFX
$ pgrep Arena
71623 /Applications/Resolume Arena/Arena.app/Contents/MacOS/Arena
$ Arena layer 23 "LoadEhr W13 Once Follows": 2 clips, nothing playing on 23 layers. tools/arena_w13.py get 23 1 / 2:
  Model B            ParamFile = /Users/amritrosell/Documents/Resolume Arena/MaskEhr Library/Skulls/Goat Skull/scene.gltf
  C Auto Place       ParamChoice = Once Per Model
  C Enable           ParamBoolean = True;   C Auto Place       ParamChoice = Every Model Swap
$ ls _Commons/ARENA-HELD-BY.txt
ls: ../_Commons/ARENA-HELD-BY.txt: No such file or directory
```
END OF HANDOFF w13-once-follows sha256:acdc5070 9368
