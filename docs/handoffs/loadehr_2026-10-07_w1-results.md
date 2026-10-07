project:    loadehr
pass:       w1-results
STATUS:     COMPLETE
parent:     c7ced33 (MaskEhr, the copy source; LoadEhr has no earlier commit)
result:     3936c34138f3ac25c691cc6523039d5390b6328f
session:    4b964771-8c73-47e3-8148-97956e9c25c7 (LoadEhr Claude Code)
written_at: 2026-10-07T23:19:38Z

# LoadEhr W1 results — MaskEhr without lasers or a library, four model slots that morph in a chain

## What shipped
- New private repo AmritusG/loadehr (main), folder ~/Documents/claude/ffgl/LoadEhr, copied from MaskEhr c7ced33 with timestamps
  kept, minus .git / build / verify / the stray "-" file / the carnival mask / test models. MaskEhr was only read (its reference
  build went to a scratch folder; `git status` in MaskEhr unchanged).
- Four bundles: LoadEhr (LEhr), LoadEhr FX (LEfx), LoadEhr Wow (LEwo), LoadEhr Wow FX (LEwf). IDs checked: no collision in
  Extra Effects. Universal (arm64 + x86_64), no warnings in our code (third-party only). Installed; Arena restarted by Amrit,
  PID 95166 -> 31243.
- WENT: the Lasers group and every laser path (Laser.cpp, LaserShaders.h, mirror-room beams, the laser prewarm, clocks, params,
  recipes C1-C3 / C27 / C28 / T Lasers, wraps rows); the library (Library.*, Library + Library B params, rescan, category /
  "Not For Shows" / "_" rules, Sketchfab / credit / prune / sheets / orient tools, w17 / w18 / eyes_all sweeps); dead W1 scripts
  that set params by index (w1_looks / fx / agnostic / motion: they addressed W1's layout, dead in MaskEhr since W2) and
  w25_libb; the wave Arena dialers (w22/24/25/26_arena).
- STAYED: every other feature (Volume, Smoke Sim, Trip, Fire, Lightning, Swarm, Lava, Caustics, Mirror Room, Lights A/B/C + eye
  finder, Spots, Projector, Post, Floor, Silhouette, Shape / Copies / Letters, Camera / Camera Live, FX + Face, Audio, Debug),
  Text / Logo, orient.txt (read by exact path next to the chosen file).

## Slots + chain (design)
- Model A..D (FILE) each with Status + Reload. Model A = MaskEhr's Model File (Model Source still applies to A). Every filled
  slot loads + uploads in the background; per-slot caches built ahead of use: shards, dust, liquid SDF (always, so switching to
  Liquid never waits), sim obstacle SDF, fit glide, eye search.
- Transition walks the filled slots A->B->C->D (equal share per hop; whole points = pure models); Chain Loop adds D->A. Style
  Order Fixed / Cycle / Random (hash of the hop: stable while scrubbing). Transition Play Once / Ping-Pong / Loop with Transition
  Time per hop + Hold Time per arrival (2 slots + Hold 0 = MaskEhr's play exactly).
- Dominant model (nearer of the pair) without a pop: shadow casters and the floor mirror draw BOTH models mid-hop with their own
  half of the transition (nothing switches); the sim obstacle switches at t 0.5 with no obstacle velocity that frame; eye
  openings glide from one model's eyes to the next across t 0.35-0.65; swarm homes / caustics / lightning / lava seeds follow
  the dominant model. Parts: values stay per part slot on every model (as Model B always was); the Parts group is RELABELLED
  after the dominant model at each hop's midpoint (unused slots hide) — that is what Amrit sees there as the chain moves.
- Clones follow the incoming model too (MaskEhr's Model B had none). Arena re-trigger = timeline animation on Transition.

## Validated (numbers)
- Params / bundles: LoadEhr 507, FX 511, Wow 493, Wow FX 507 (all <= 512). Lasers -24, library -2, slots +12 (replacing 4),
  chain +3; FX hit 521, so Face (10, FX only) moved to the Wow carrier. Four bundles = fewest (union ~650 params; source and
  effect are separate plugin types).
- Parity, 80 items (66 looks + 13 harness scripts + w2), shots at 1/2/3/5 s in arena AND black composites, 680 shots, LoadEhr x2-4
  runs vs MaskEhr x2-5 runs: identical 38, float noise (max <= 2/255) 8, within MaskEhr's own run-to-run spread 34, DIFFERS 0.
  Strips (MaskEhr / LoadEhr / diff x8): docs/parity/ (80 jpg, 10 MB).
- Chain: styles x {2, 3, 4 slots, 4 + Chain Loop, 2 + Chain Loop} scrubbed in 9 steps (verify/w1/chain/styles/styles.png); play
  modes give A~BBB CCC~DDD (Once holds D; Ping-Pong returns; Loop cuts D->A; Chain Loop morphs D->A); scrubbing BACKWARD is
  bit-identical to forward at 13 points for all five styles (max diff 0).
- Cost (harness, 1080p, Wow, 4 slots, plugin wall ms avg): rest 1.6-3.3, mid-hop 1.7-3.0 per style (verified, harness only).
- No frame waits on a load: 3 heavy 8K/4K models loaded while the 4-slot chain played (realtime): worst frame 27 ms (once per
  model, its first prewarm), upload worst step 7.8 ms; once loaded max 16 ms. GPU per slot with 4 heavy models: Corinthian
  1026 MB, Crown 1918 770 MB, Kabuto 546 MB, knot 0.6 MB, + 7.5 MB per-slot caches = 2373 MB total.
- Wraps: 50 clock wraps ok incl. Chain Loop / Chain Loop + Hold / Chain Ping-Pong; 180 min: chain per-frame change 17.55 -> 17.47
  max (no drift); motion and surface scenes flat too.
- Guards G1-G5 pass on all four bundles; regress: 6 references (generated models), max 0, ref-hash pinned.

## Deviations (and why)
- Carnival mask: Amrit mid-session: "Carnival mask is absolutely forbidden to use!" It is now banned outright (CLAUDE.md rule 3,
  parity refuses it). All clips moved off it (Plague Mask by path), the Arena instances re-rendered via thumbnail refresh (no
  trigger) and dropped it, every local render made with it deleted and re-rendered; MaskEhr recipes on it use
  Masks/Plague Mask on BOTH sides of the parity. Memory written so no session uses it again.
- Shadows mid-hop draw both models instead of switching at the midpoint (no pop at all); parity looks are unaffected (they
  have no hops).
- Fixed two latent issues found on the way: re-warming every program at every hop (25-37 ms hitch per hop: now each model is
  warmed once), and a harness bug (FFT index cached across bundles: FF_FAIL read as 4 billion elements).
- The installed bundles predate the last commit by cosmetic edits only (DeinitLibrary -> DeinitBundle, comments); behaviour
  identical (regress max 0 both).
- Not committed: verify/, test_models/ (DamagedHelmet is CC-BY, local), the generated models (tools/gen_test_models.py makes them).

## Arena (comp "LoadEhr", not saved; clips untriggered, selection on layer 3 column 9)
- Layer 1 "MaskEhr" (originals): C1 Dragon Breath, C2 Mirror Fire, C3 Liquid Morph 2.
- Layer 2 "LoadEhr": C1 Dragon Breath, C2 Mirror Fire, C3 Liquid Morph 2 (parity, same columns); C4 Chain Liquid Loop (4 slots,
  Liquid, Chain Loop, Loop, 8 s/hop, hold 2 s); C5 Chain Cycle Ping-Pong (3 slots, Style Order Cycle, 6 s, hold 1.5 s); C6 Chain
  Shatter Once (2 slots, Shatter, Arena-side timeline 6 s, Play Once & Clear, Restart; transport Play Once & Hold 6 s);
  C7 Chain Slow Dissolve (4 slots, Dissolve, Chain Loop, 12 s/hop, hold 5 s). Models by path: Plague Mask, Human Skull, Diving
  Helmet, Crystal Cluster. Soft: Spin Y 4. C3 pair = Arena-side timeline 6 s as MaskEhr W28.
- At report time C7 was PLAYING (connected): not triggered by this session.

## Open questions / for Amrit
1. libtess2 (text / logo extrusion) is SGI Free Software License B, not on your list. Proposed replacement: Clipper2 (Boost
   licence: resolves the fill rule) + earcut (ISC: triangulates). Replace it, or keep libtess2?
2. Four slots or two? (Cowork's pick under add-then-prune.)
3. The Parts group relabels after the dominant model at each hop: keep, or label after Model A always?

## Amrit's answers this session
- Arena restart: "Arena restarted" (PID 95166 -> 31243).
- "Carnival mask is absolutely forbidden to use!" (recorded as hard rule 3; overrides the brief).

## Commits
- 3936c34 LoadEhr W1 (pushed, main...origin/main). This handoff: the commit after it.

## RECEIPTS
```
$ git log -1 --stat | head -3 ; git status -sb
commit 3936c34138f3ac25c691cc6523039d5390b6328f
Author: Amrit Rosell <amrit.rosell@gmail.com>
Date:   Thu Oct 8 01:15:20 2026 +0200
## main...origin/main

$ tools/regress.sh w1/regress_after (compares + guards)
compare ref/r1_mask_front.png vs regress_after/r1_mask_front.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare ref/r2_wireframe50.png vs regress_after/r2_wireframe50.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare ref/r3_explode.png vs regress_after/r3_explode.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare ref/r4_fx_skin_env_input.png vs regress_after/r4_fx_skin_env_input.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare ref/r5_chain_mid_hop_b_c.png vs regress_after/r5_chain_mid_hop_b_c.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare ref/r6_chain_rest_d.png vs regress_after/r6_chain_rest_d.png: mean 0.0000 max 0 pixels>2 0 of 2073600
G1 ok   LoadEhr: 507 params, none named Library* / Laser*
G4 ok   LoadEhr: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model
G1 ok   LoadEhrFX: 511 params, none named Library* / Laser*
G4 ok   LoadEhrFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a mod
G1 ok   LoadEhrWow: 493 params, none named Library* / Laser*
G4 ok   LoadEhrWow: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a mo
G1 ok   LoadEhrWowFX: 507 params, none named Library* / Laser*
G4 ok   LoadEhrWowFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a 
G2 ok   the only directory API: ModelLoader.cpp FolderIndex (textures next to the chosen model file):
        src/ModelLoader.cpp:88:		DIR* d = opendir( dir.c_str() );
        src/ModelLoader.cpp:91:		while( dirent* e = readdir( d ) )
G3 ok   no model / texture file inside build/LoadEhr*.bundle (8 files: binaries + Info.plist)
G5 ok   no Laser* / Library* sources, no library tools

$ for b in build/LoadEhr*.bundle (lipo)
Architectures in the fat file: build/LoadEhr.bundle/Contents/MacOS/LoadEhr are: x86_64 arm64 
Architectures in the fat file: build/LoadEhrFX.bundle/Contents/MacOS/LoadEhrFX are: x86_64 arm64 
Architectures in the fat file: build/LoadEhrWow.bundle/Contents/MacOS/LoadEhrWow are: x86_64 arm64 
Architectures in the fat file: build/LoadEhrWowFX.bundle/Contents/MacOS/LoadEhrWowFX are: x86_64 arm64 

$ ls -la "~/Documents/Resolume Arena/Extra Effects" | grep LoadEhr
drwxr-xr-x@  3 amritrosell  staff     96 Oct  7 20:05 LoadEhr.bundle
drwxr-xr-x@  3 amritrosell  staff     96 Oct  7 20:05 LoadEhrFX.bundle
drwxr-xr-x@  3 amritrosell  staff     96 Oct  7 20:05 LoadEhrWow.bundle
drwxr-xr-x@  3 amritrosell  staff     96 Oct  7 20:05 LoadEhrWowFX.bundle

$ ls -la docs/parity | head -4; du -sh docs/parity; ls docs/parity | wc -l
total 20464
drwxr-xr-x  82 amritrosell  staff    2624 Oct  8 00:51 .
drwxr-xr-x   6 amritrosell  staff     192 Oct  8 00:48 ..
-rw-r--r--   1 amritrosell  staff   21708 Oct  8 00:48 script_smoke.jpg
 10M	docs/parity
      80

$ tools/parity.py report verify/parity
looks: 80, shots: 680 (each look at its worst shot)
  identical                            38
  float noise (max <= 2)               8
  noise (within MaskEhr run-to-run)    34

$ Arena REST (comp "LoadEhr")
comp: LoadEhr
layer 1 'MaskEhr'
  C1 Dragon Breath          Disconnected
  C2 Mirror Fire            Disconnected
  C3 Liquid Morph 2         Disconnected
layer 2 'LoadEhr'
  C1 Dragon Breath          Disconnected
  C2 Mirror Fire            Disconnected
  C3 Liquid Morph 2         Disconnected
  C4 Chain Liquid Loop      Disconnected
  C5 Chain Cycle Ping-Pong  Disconnected
  C6 Chain Shatter Once     Disconnected
  C7 Chain Slow Dissolve    Connected
layer 3 'Layer #'
  C9 SELECTED (empty slot)
clips: 10

$ tools/w14_wraps.py times (chain rows)
chain 1 min max 17.546 | 13: 17.538 | 26: 17.529 | 60: 17.515 | 180: 17.471 (median 0.625 throughout)
```
END OF HANDOFF w1-results a0d34623 11583
