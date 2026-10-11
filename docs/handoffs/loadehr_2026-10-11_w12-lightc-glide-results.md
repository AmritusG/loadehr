project:    LoadEhr
pass:       w12-lightc-glide
STATUS:     COMPLETE
parent:     c621216
result:     7b757c0 efb233f
session:    7fb29fc7-1678-4bc4-ba23-11a2293a5935
written_at: 2026-10-11T12:43:23Z

# LoadEhr W12: Light C "Every Model Swap" glides cone and depth (COMPLETE)

Completes docs/handoffs/loadehr_2026-10-11_w12-lightc-glide-results_CHECKPOINT.md (e638c5d): read it for the design, the deviation
and the measured tables; this doc adds the parked Arena steps and fresh gates.

## Your words (verbatim, 2026-10-11)
"Lights can glide, looks more natural." Then, this session: "Arena restarted." / "lock is yours" / "Arena restarted."

## What shipped
- Code: 7b757c0 (the glide), tests tools/w12_glide.py; Arena dialer tools/arena_w12.py (efb233f).
- Installed all four bundles (after the lock was handed over; the first restart came before the install, so a second one was needed).
  Installed vs build/, signatures stripped: 2 bytes differ (offset 18426-18427, the signature load command's size in the slice header),
  code identical. Arena PID 3910 -> 46785 (restart 1, old plugin) -> 97391 (restart 2, W12 plugin).
- Arena: NEW layer 22 "LoadEhr W12 Light Glide", opacity 1, UNTRIGGERED:
  C1 (column 1) "W12 C1 Every Model Swap (glide)", C2 (column 2) "W12 C2 Once Per Model (compare)". Both: skull A -> Damaged Helmet B,
  Transition Play Ping-Pong, Time 6, Hold 1.5, Haze 0.25, Smoke 0, Orbit Yaw 60, C dials preset off-aim (At Camera, cone 70, Behind 1.5,
  not anchored), then C Auto Place, then C Enable. The finder places on first play (positions still at defaults until then).
  No other layer or clip touched; nothing playing; I did not save the comp (layer 22 is on disk only once you save).
- Lock: taken over from Fanous W3 with your OK, removed when done.

## Validated
- Glide (CHECKPOINT doc, unchanged code): skull -> Damaged max step 0.284 (W11 0.861), skull -> Goat 0.219 (W11 5.00), interrupted hop
  0.138 per frame (W11 6.54); arrival readback = W11; Off / Once / Position renders max 0 vs W11; audit DEAD / BROKEN / MISROUTED 0.
- Fresh this session: regress 6/6 max 0, guards G1-G7 ok, counts 487 / 508 / 497 / 510.
- NOT done: watching C1 / C2 play in Arena (untriggered by rule). Your eyes: trigger C1, then C2, and compare the beams at each swap.

## Open
- The revisit deviation (a model gets its own fit back, W11 kept the lowered Behind Depth): your call.

## RECEIPTS
```
$ git log -1 ; git status -sb
efb233f6b108e0f1a6fbcfff2d802e55465c38ba W12: Arena Light Glide clips (tools/arena_w12.py), installed + dialed
## main...origin/main
$ tools/regress.sh w12r   (incl. tools/guards.sh)
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
G7 ok   no LGPL file in the tree (1046 tracked files, submodules included)
$ ls -la verify/w12r ; du -sh  (cleared after this handoff)
total 6976
drwxr-xr-x  10 amritrosell  staff      320 Oct 11 14:42 .
drwxr-xr-x   4 amritrosell  staff      128 Oct 11 14:42 ..
-rw-r--r--   1 amritrosell  staff   268451 Oct 11 14:42 r1_mask_front.png
-rw-r--r--   1 amritrosell  staff  1027046 Oct 11 14:42 r2_wireframe50.png
-rw-r--r--   1 amritrosell  staff  1422743 Oct 11 14:42 r3_explode.png
-rw-r--r--   1 amritrosell  staff   297800 Oct 11 14:42 r4_fx_skin_env_input.png
-rw-r--r--   1 amritrosell  staff   372455 Oct 11 14:42 r5_chain_mid_hop_b_c.png
-rw-r--r--   1 amritrosell  staff   165019 Oct 11 14:42 r6_chain_rest_d.png
-rw-r--r--   1 amritrosell  staff     1712 Oct 11 14:42 regress.log
-rw-r--r--   1 amritrosell  staff     1999 Oct 11 14:42 regress_script.txt
3.4M	verify/w12r
$ ls -la Extra Effects/LoadEhr*.bundle/Contents/MacOS/*
-rwxr-xr-x  1 amritrosell  staff  4879168 Oct 11 14:38 /Users/amritrosell/Documents/Resolume Arena/Extra Effects/LoadEhr.bundle/Contents/MacOS/LoadEhr
-rwxr-xr-x  1 amritrosell  staff  4935104 Oct 11 14:38 /Users/amritrosell/Documents/Resolume Arena/Extra Effects/LoadEhrFX.bundle/Contents/MacOS/LoadEhrFX
-rwxr-xr-x  1 amritrosell  staff  4862592 Oct 11 14:38 /Users/amritrosell/Documents/Resolume Arena/Extra Effects/LoadEhrWow.bundle/Contents/MacOS/LoadEhrWow
-rwxr-xr-x  1 amritrosell  staff  4934960 Oct 11 14:38 /Users/amritrosell/Documents/Resolume Arena/Extra Effects/LoadEhrWowFX.bundle/Contents/MacOS/LoadEhrWowFX
$ pgrep Arena
97391 /Applications/Resolume Arena/Arena.app/Contents/MacOS/Arena
$ Arena layer 22 "LoadEhr W12 Light Glide": 2 clips, 0 playing (composition playing: nothing on 22 layers). tools/arena_w12.py get 22 1 / 2:
  C Auto Place       ParamChoice = Every Model Swap
  C Enable           ParamBoolean = True
  C Left Pos X       ParamRange = -0.30000001192092896
  C Placed For       MISSING (hidden TEXT param: not in REST)
-- C2:
  C Auto Place       ParamChoice = Once Per Model
  C Enable           ParamBoolean = True
$ comp file (14:40 = Arena's save at your cmd-Q, BEFORE layer 22 was built ~14:41: layer 22 lives in the open comp until you save)
-rw-r--r--  1 amritrosell  staff  9735980 Oct 11 14:40 /Users/amritrosell/Documents/Resolume Arena/Compositions/LoadEhr.avc
$ ls _Commons/ARENA-HELD-BY.txt
ls: ../_Commons/ARENA-HELD-BY.txt: No such file or directory
```
END OF HANDOFF w12-lightc-glide sha256:9926e133 7025
