project:    LoadEhr
pass:       w7-transition-prune
STATUS:     COMPLETE
parent:     bebfeac
result:     86556eb a680c2e 3533a3c
session:    session_01LxGX4yxKucdPVDMKd9cfmG
written_at: 2026-10-08T20:06:51Z
# LoadEhr W7 results: the Twist / Melt glitch fixed, Twist and Melt rebuilt, Trans Sparkle, Lava on the skull, Fold + Shrinkwrap out

## Amrit's words (2026-10-08, verbatim, from the brief)
- On W6 open question 1 (InitGL +0.6 s): "Fine."
- On the transitions: "Twist and Melt has this glitch. Can the twist be more twisted and the melt be more melted and perhaps a sparkle
  that kicks in between the models shifting? Lava didn't work on the skull, so it look weird. Fold can go and also Shrink Wrap. Rest stays."
- This session: Amrit restarted Arena when asked (PID 2462 -> 53933). No other questions were asked.

## Diagnosis 1: the Twist / Melt glitch (proven before any fix)
- **Where:** in the MIDDLE of the hop, not at its ends.
  - Scrubbed in 400 steps a way on Arena's C8 dials (ram skull -> crown): the first step off rest changes 0.0000 mean (max 1).
  - Per-step change spikes at t 0.49 .. 0.52: 13.4 mean, 22k of 230k pixels changed by > 32. The hop's own worst elsewhere is 2.1.
  - Same in both directions; Melt the same (11.8 vs 1.9).
  - So the brief's prime suspect (the plain -> DEFORM-twin switch at the ends) is falsified.
- **What:** the mid-hop swap. Each side alone (LOADEHR_TRANS_SOLO) shows the whole surface trading places within t 0.48 .. 0.53: outgoing
  model 22.2k -> 0 px, incoming 0 -> 54k px. That is ~0.3 s of a 6 s hop, between a model twisted one way and one twisted the other.
- **Why:** the swap cut is a threshold on the dissolve noise (an Fbm), moved evenly over e 0.4 .. 0.6. The noise clusters near 0.5, so
  nearly all of the surface crosses the threshold in the middle ~15 % of that window.
- **Falsifying test:** cut by the noise's measured quantiles on each model. The spike fell to 6.2 (16 quantiles: the skewed tails still
  popped 1/16 of the surface in one step), then to the level of the hop's own motion with 128 quantiles.

## Fix 1, and the new Twist / Melt
- **Cut()** walks each model's own noise quantiles (129, from 16k area-weighted surface points, built on the load worker). Every style
  that swaps through the dissolve uses it; the Dissolve style itself is unchanged.
- **Twist** (rebuilt, MORPH mode 7; "more twisted"):
  - Winds round the Deform Axis: the ends turn +-pi (1 + 1.5 Spread). Spread 0.6 = 1.9 turns end to end; Spread 4 = 7 turns.
  - Wrung thin as it winds: down to 0.2 of its width at the top end.
  - The leading end goes first: Trans Stagger above 0.5 = the top, below 0.5 = the bottom; |Stagger - 0.5| x 2 = how far ahead.
  - The incoming model arrives wound the SAME way and unwinds. In W6 the two were twisted opposite ways.
- **Melt** (rebuilt, mode 8; "more melted"):
  - Sags, the Drip columns run ahead, and it collapses into a puddle on the floor plane (Floor Height) by e 0.4.
  - The puddle's shape depends only on the direction and the radius share, so every model's puddle is identical. The swap is a colour
    cross-fade inside it, and the other model rises out of it.
  - Spread = runny + puddle size (1.1 + 0.6 Spread fitted units); the top end floods the floor on purpose.
- **Normals:** the finite-difference normals now carry the file's own tangent. The squared tangent changed normal-mapped shading at a
  hop's first / last step (max 75 / 255 in W6's note). The same fix went into the DEFORM twin: a deform's first step on the helmet went
  821 -> 545 pixels > 2; the rest is real sub-pixel motion.

**Evidence**
- Swap window vs the hop's own worst step, both directions (`w7_trans.py spikes`, verify/w7/spike2):

| pair | Twist | Melt |
|---|---|---|
| skull -> helmet | 0.97 | 0.78 |
| text -> skull | 0.93 | 0.83 |
| ram -> crown | 0.91 / 0.94 | 0.24 |

  - W6 was 6.1x on ram -> crown.
  - This run used the final windows, before the tangent change (tangent-only).
- Arena's own C8 / C9 dials played in the harness (Ping-Pong 6 s, Hold 1.5 s, Spin Y 3, 60 Hz, one 15 s cycle), max per-frame change:
  - Twist: W6 15.4 -> W7 2.72.
  - Melt: W6 13.5 -> W7 2.60.
  - p99 2.5 .. 2.6; frames above 4.0: 16 / 10 -> 0 / 0.
- End-pop proof on all 18 remaining pack styles x 3 pairs (verify/w7/strips_final.txt):
  - Forward = backward: max 0 at every step.
  - t 0 / 1 vs the plain models: max 0.
  - Onsets (t 0.01 / 0.99 vs plain): Twist max 3 / 20, Melt mean <= 0.004 max <= 36. The other styles: Glitch 34, Pieces 42,
    Spin Swap 65, the rest <= 21.
  - (The live-clock scrub differs forward / backward by <= 0.005 mean at <= 5 of 400 steps; the zeroed-clock strips are exact.)

## Diagnosis 2: Lava on the skull, and its fix
- **Arena's C16 was not skull -> crown:** it is Nefertiti -> Deer Skull, Ping-Pong, so the skull is Model B.
- **Proven:**
  - (a) Only the OUTGOING model ever cracked. Lava Glow 3 vs 0 changes 0 px on the incoming side at t 0.3 / 0.5 / 0.7, against
    12.8-19.7k on the outgoing side. So the skull never cracked in either direction of Ping-Pong.
  - (b) The incoming model arrived as white blotches: 22-32 % of its pixels near-white at t 0.55-0.6 (0 % at rest). That is the
    dissolve edge glow.
  - (c) With the skull outgoing, Lava Split tore its thin antlers into floating bits.
- **Fix:** both models carry the lava.
  - The outgoing model cracks (front by e 0.45), splits open (0.6 x the W26 split) and dissolves away (0.5 .. 0.9).
  - The incoming model shows through from 0.25, still split and glowing, then closes, heals and cools.
  - No dissolve edge glow in this style; the cracks are the edge.
  - Strips: skull -> crown, Helmet -> skull, Nefertiti <-> Deer Skull, text -> helmet (verify/w7/lava_fix, final_strips/all.png).
- **Classic bundles:** still Dissolve (the Lava dials are Wow-only).

## Trans Sparkle (new; Model group, every bundle, 0..10, default 0)
- **Where it sits:** on both models' surfaces, riding the twisted / melted / split / flying surface. Each sparkle set is a Points Only,
  glow-only draw (Surface 25k, a share of it), so it uses no new shader.
- **Light:** into the W5 HDR wire buffer at alpha 0, so Wire Bloom and the global Bloom catch it. Colour = the surface lifted halfway to
  white; twinkles 3 / s (W14-safe: x 3 of the Point Twinkle clock).
- **Envelope per style (eased progress):** up from 0.2 before the swap point, peak at it, gone 0.2 after. Swap point 0.5 for every style
  except Lava 0.55.
- **Spin Swap / Implode:** each model's sparkles dim with what is left of it; otherwise they piled into one white dot.
- **At 0:** no draw, no pass (Dissolve mid-hop with Sparkle 0 read 1.13 and 1.07 GPU ms in two rows of the same run: noise).
- **Params:** LoadEhr 504, FX 508, Wow 498, Wow FX 512 (exactly the cap). Nothing moved, nothing cut.

## Removed: Fold and Shrinkwrap
- Both were retired as TOMBSTONES per CONTRACT 7.2a / TOROFIELD-050: not in the dropdown, their values 6 / 22 kept, so every other
  style keeps its number. A clip holding either value dissolves.
- Cycle / Random go round the 21 listed styles.
- Gone with them: MORPH vertex mode 1, the 3D SDF sampler on unit 27 and its dummy texture, both style cases, the shrinkwrap prewarm
  (WarmMorph now warms mode 7). Deform's Mirror Fold stays.

## Costs
- **Harness 1080p, GPU ms mid-hop (t 0.5), skull -> helmet, Wow:**
  - Rest 0.64; Dissolve 1.13; Twist 1.54 (W6 1.75); Melt 1.31 (W6 1.59); Lava 3.00 (W6 2.48: both models crack now); Points 1.25;
    Spin Swap 0.93.
  - + Sparkle 2 / 10: Dissolve 2.20 / 2.37, Twist 3.05 / 3.13, Points 2.78 / 2.93, Spin Swap 1.05 / 1.27. So about +1.1 .. 1.5 ms while
    it sparkles (the wire buffer's depth pass of both models + the points).
- **Arena 7.28, MacBook Pro M3 Max, Wire closed, one clip selected (preview), nothing triggered:**
  - Every W7 clip delivered 84-97 fps median in this run (Arena's 80-100 fps regime; the same Twist clip read 97 then 84 on two runs).
  - Sparkle 10 read 94: inside that spread, so no cost resolvable in Arena.
  - Plugin CPU 0.66-0.94 ms median.

## Unchanged (checked)
- Regress max 0 on all 6 references.
- G1-G7 ok on all four bundles.
- W5: the 48 wire renders max 0 vs verify/w5 (a first run read 209 on one Feature Edges shot while 3 other harness jobs ran: the edge
  flags land on a worker; alone it is 0).
- W6 points renders: max 0 vs the W6 HEAD build (11 shots).
- W3 DXV proof: max 2-4, as in W6.
- W4 sweep: DXV / HAP / HAP Q at 1080 + 4K show every wanted frame in order with 0 wrong (only the "in between" counts on 4K drags vary
  with load: decoder timing; video code untouched).
- No new clock: the sparkle twinkle rides the Point Twinkle clock.

## Arena clips (layer 12 "LoadEhr W7", LoadEhr Wow, opacity 1; untriggered; selection parked on L12 C13)
- **Shared dials:** Ping-Pong, 6 s per hop, Hold 1.5 s, Spin Y 3, Key 0.8 (`tools/arena_w7.py 12`).
- **C1** W7 Twist Default (ram skull -> Crown 1918, Spread 0.6).
- **C2** W7 Twist Top End (the same pair, Spread 4, Stagger 1).
- **C3** W7 Melt Default (Ganesha -> Human Skull).
- **C4** W7 Melt Top End (Spread 4, Drip 3).
- **C5** W7 Lava Skull Crown (Human Skull -> Crown 1918).
- **C6** W7 Lava Helmet (SciFi Helmet -> Human Skull).
- **C7** W7 Sparkle Points (skull -> helmet, Sparkle 3).
- **C8** W7 Sparkle Spin Swap (Nefertiti -> crown, Sparkle 3).
- **C9** W7 Sparkle Dissolve (Anubis -> skull, Sparkle 3).
- **C10** W7 Sparkle 10 (Twist skull -> helmet, "too much").
- **C11** W7 Cycle 21 Sparkle 2 (4 slots: skull, helmet, crown, Nefertiti; Chain Loop, Loop, Cycle).
- Earlier layers were not touched. No model path contains "carnival".

## Deviations
- **Tombstones instead of deleting the two entries:** the contract forbids renumbering an enum. The dropdown looks the same as deleting
  them.
- **The sparkle shares the wire glow buffer.** Edge case: with Wire Glow 0 and Wire Bloom > 0, mid-sparkle the wire buffer adds at
  Glow 1, so the lines get some direct light too.
- **The DEFORM twin's tangent fix** goes beyond the brief: a W2 deform on a normal-mapped model now shades slightly differently (no
  shading jump as a deform dial leaves 0).

## Open, for Amrit's eyes
1. Twist / Melt top ends: are they "too much" in the right way (C2, C4)?
2. Sparkle default strength: C7-C9 sit at 3, C11 at 2. Is 3 the right everyday level?

## Commits
86556eb (code), a680c2e (clip dials, tools, docs), + this handoff. Commons: LOADEHR-014 (noise-cut quantiles), LOADEHR-015 (FD tangent).

## RECEIPTS
```
$ git log -1 ; git status -sb ; git log --oneline bebfeac..HEAD
3533a3c8b16ce69d8ead88c934ab4ef6ac99650f W7: final real-time numbers in CLAUDE.md
## main...origin/main
3533a3c W7: final real-time numbers in CLAUDE.md
a680c2e W7: clip dials (tools/arena_w7.py), w7_trans spikes / realtime / cost, CLAUDE.md tool lines
86556eb LoadEhr W7: the transition review (Twist / Melt glitch fixed, Twist / Melt rebuilt, Trans Sparkle, Lava on both models, Shrinkwrap + Fold retired)

$ tools/regress.sh w7/regress_final   (final build)
compare verify/w1/ref/r1_mask_front.png vs verify/w7/regress_final/r1_mask_front.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r2_wireframe50.png vs verify/w7/regress_final/r2_wireframe50.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r3_explode.png vs verify/w7/regress_final/r3_explode.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r4_fx_skin_env_input.png vs verify/w7/regress_final/r4_fx_skin_env_input.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r5_chain_mid_hop_b_c.png vs verify/w7/regress_final/r5_chain_mid_hop_b_c.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r6_chain_rest_d.png vs verify/w7/regress_final/r6_chain_rest_d.png: mean 0.0000 max 0 pixels>2 0 of 2073600
G1 ok   LoadEhr: 504 params, none named Library* / Laser*
G6 ok   LoadEhr: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhr: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G1 ok   LoadEhrFX: 508 params, none named Library* / Laser*
G6 ok   LoadEhrFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G1 ok   LoadEhrWow: 498 params, none named Library* / Laser*
G6 ok   LoadEhrWow: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWow: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G1 ok   LoadEhrWowFX: 512 params, none named Library* / Laser*
G6 ok   LoadEhrWowFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWowFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G2 ok   the only directory API: ModelLoader.cpp FolderIndex (textures next to the chosen model file):
G3 ok   no model / texture file inside build/LoadEhr*.bundle (8 files: binaries + Info.plist)
G5 ok   no Laser* / Library* sources, no library tools
G6 ok   no Mirror* source, no MIRROR program variant
G7 ok   no libtess2 / tesselator / SGI-B in src, third_party, CMakeLists.txt
G7 ok   no LGPL file in the tree (630 tracked files, submodules included)

$ tools/w6_trans.py strips verify/w7/strips_final   (pair | style | fwd=bwd | t0 | t1 | onsets)
 skull_helmet | 5 Points | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 0 
 skull_helmet | 7 Wire | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 1 / 0.000, 1 
 skull_helmet | 8 Voxel | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 6 
 skull_helmet | 9 Low Poly | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 6 
 skull_helmet | 10 Pieces | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 6 
 skull_helmet | 11 Implode | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 1 
 skull_helmet | 12 Twist | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 1 
 skull_helmet | 13 Melt | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.001, 27 / 0.001, 21 
 skull_helmet | 14 Scan | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 1 
 skull_helmet | 15 Glitch | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.002, 34 / 0.000, 15 
 skull_helmet | 16 Ripple | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 7 
 skull_helmet | 17 Swarm (classic: dissolves) | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 0 
 skull_helmet | 18 Smoke | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 0 
 skull_helmet | 19 Burn (classic: dissolves) | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 0 
 skull_helmet | 20 Lava (classic: dissolves) | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 0 
 skull_helmet | 21 Spin Swap | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.001, 10 / 0.001, 21 
 text_skull | 5 Points | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 0 
 text_skull | 7 Wire | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 1 / 0.000, 1 
 text_skull | 8 Voxel | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 3 
 text_skull | 9 Low Poly | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 1 
 text_skull | 10 Pieces | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.001, 42 / 0.000, 3 
 text_skull | 11 Implode | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 3 
 text_skull | 12 Twist | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 1 / 0.000, 3 
 text_skull | 13 Melt | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 1 / 0.001, 27 
 text_skull | 14 Scan | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 3 
 text_skull | 15 Glitch | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 27 / 0.001, 31 
 text_skull | 16 Ripple | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 3 
 text_skull | 17 Swarm (classic: dissolves) | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 0 
 text_skull | 18 Smoke | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 0 
 text_skull | 19 Burn (classic: dissolves) | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 0 
 text_skull | 20 Lava (classic: dissolves) | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 0 
 text_skull | 21 Spin Swap | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 38 / 0.001, 11 
 skull_crown | 5 Points | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 0 
 skull_crown | 7 Wire | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 1 / 0.000, 1 
 skull_crown | 8 Voxel | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 20 
 skull_crown | 9 Low Poly | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 20 
 skull_crown | 10 Pieces | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 20 
 skull_crown | 11 Implode | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 20 
 skull_crown | 12 Twist | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 20 
 skull_crown | 13 Melt | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.001, 27 / 0.004, 36 
 skull_crown | 14 Scan | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 20 
 skull_crown | 15 Glitch | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.002, 34 / 0.003, 33 
 skull_crown | 16 Ripple | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 20 
 skull_crown | 17 Swarm | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 0 
 skull_crown | 18 Smoke | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 0 / 0.000, 0 
 skull_crown | 19 Burn | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 20 
 skull_crown | 20 Lava | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.000, 3 / 0.000, 20 
 skull_crown | 21 Spin Swap | 0 0 0 0 0 0 0 0 0 (worst 0) | max 0 | max 0 | 0.001, 10 / 0.004, 65 

$ tools/w7_trans.py realtime (Arena C8 / C9 dials, final build; frame 0 = the harness clock reset, excluded)
ram_crown Twist _w6: median 0.294 p99 9.178 max 15.395 at 10.78 s, frames > 4.0: 16
ram_crown Twist _final: median 0.298 p99 2.579 max 2.716 at 10.07 s, frames > 4.0: 0
ganesha_skull Melt _w6: median 0.098 p99 7.562 max 13.525 at 3.00 s, frames > 4.0: 10
ganesha_skull Melt _final: median 0.141 p99 2.497 max 2.601 at 3.97 s, frames > 4.0: 0

$ cat verify/w7/spike2/spikes.md
| pair | style | fwd swap / own | bwd swap / own | fwd = bwd (step for step) |
|---|---|---|---|---|
| skull_helmet | Twist | 1.210 / 1.246 (0.97) | 1.210 / 1.246 (0.97) | True |
| skull_helmet | Melt | 1.506 / 1.936 (0.78) | 1.506 / 1.937 (0.78) | False |
| text_skull | Twist | 1.169 / 1.262 (0.93) | 1.169 / 1.262 (0.93) | False |
| text_skull | Melt | 1.604 / 1.936 (0.83) | 1.604 / 1.936 (0.83) | False |
| ram_crown | Twist | 2.203 / 2.414 (0.91) | 2.262 / 2.414 (0.94) | True |
| ram_crown | Melt | 1.043 / 4.295 (0.24) | 1.043 / 4.295 (0.24) | True |

$ cat verify/w7/cost/cost.md
| case | plugin ms avg | GPU ms avg | GPU ms p99 |
|---|---|---|---|
| rest (Transition 0) | 1.65 | 0.64 | 1.52 |
| Dissolve t 0.5 | 2.04 | 1.13 | 2.50 |
| Twist t 0.5 | 2.64 | 1.54 | 2.62 |
| Melt t 0.5 | 2.29 | 1.31 | 3.37 |
| Lava t 0.5 | 4.35 | 3.00 | 4.11 |
| Points t 0.5 | 2.55 | 1.25 | 3.34 |
| Spin Swap t 0.5 | 2.22 | 0.93 | 2.28 |
| Dissolve t 0.5 Sparkle 2 | 3.18 | 2.20 | 3.42 |
| Dissolve t 0.5 Sparkle 10 | 3.64 | 2.37 | 4.29 |
| Twist t 0.5 Sparkle 2 | 4.40 | 3.05 | 5.00 |
| Twist t 0.5 Sparkle 10 | 4.27 | 3.13 | 5.06 |
| Points t 0.5 Sparkle 2 | 3.95 | 2.78 | 4.53 |
| Points t 0.5 Sparkle 10 | 3.97 | 2.93 | 5.70 |
| Spin Swap t 0.5 Sparkle 2 | 1.92 | 1.05 | 2.03 |
| Spin Swap t 0.5 Sparkle 10 | 2.23 | 1.27 | 3.75 |
| Dissolve t 0.5 Sparkle 0 (rest of the sparkle code: off) | 1.96 | 1.07 | 1.86 |

$ tools/w6_points.py wire_same verify/w7/wire_same2
worst: 0 over 48 shots

$ Arena: PID + layer 12
Arena PID 53933 (was 2462)
LoadEhr W7 opacity 1.0
 C1 W7 Twist Default Disconnected
 C2 W7 Twist Top End Disconnected
 C3 W7 Melt Default Disconnected
 C4 W7 Melt Top End Disconnected
 C5 W7 Lava Skull Crown Disconnected
 C6 W7 Lava Helmet Disconnected
 C7 W7 Sparkle Points Disconnected
 C8 W7 Sparkle Spin Swap Disconnected
 C9 W7 Sparkle Dissolve Disconnected
 C10 W7 Sparkle 10 Disconnected
 C11 W7 Cycle 21 Sparkle 2 Disconnected

$ ls -la ~/Documents/Resolume\ Arena/Extra\ Effects/LoadEhr*.bundle/Contents/MacOS/
-rwxr-xr-x  1 amritrosell  staff  4604544 Oct  8 21:42 /Users/amritrosell/Documents/Resolume Arena/Extra Effects/LoadEhr.bundle/Contents/MacOS/LoadEhr
-rwxr-xr-x  1 amritrosell  staff  4676912 Oct  8 21:42 /Users/amritrosell/Documents/Resolume Arena/Extra Effects/LoadEhrFX.bundle/Contents/MacOS/LoadEhrFX
-rwxr-xr-x  1 amritrosell  staff  4604544 Oct  8 21:42 /Users/amritrosell/Documents/Resolume Arena/Extra Effects/LoadEhrWow.bundle/Contents/MacOS/LoadEhrWow
-rwxr-xr-x  1 amritrosell  staff  4676912 Oct  8 21:42 /Users/amritrosell/Documents/Resolume Arena/Extra Effects/LoadEhrWowFX.bundle/Contents/MacOS/LoadEhrWowFX

$ ls -la verify/w7 (evidence, local; selection)
drwxr-xr-x   4 amritrosell  staff    128 Oct  8 21:42 cost
drwxr-xr-x  18 amritrosell  staff    576 Oct  8 20:54 diag
drwxr-xr-x  15 amritrosell  staff    480 Oct  8 22:04 final_strips
drwxr-xr-x  35 amritrosell  staff   1120 Oct  8 21:11 lava_diag
drwxr-xr-x   8 amritrosell  staff    256 Oct  8 21:11 lava_fix
drwxr-xr-x   8 amritrosell  staff    256 Oct  8 22:05 realtime
drwxr-xr-x  10 amritrosell  staff    320 Oct  8 22:04 regress_final
-rw-r--r--   1 amritrosell  staff   2373 Oct  8 22:04 regress_final.txt
drwxr-xr-x   9 amritrosell  staff    288 Oct  8 21:19 spike2
drwxr-xr-x  54 amritrosell  staff   1728 Oct  8 21:41 strips_final
-rw-r--r--   1 amritrosell  staff   6039 Oct  8 21:41 strips_final.txt
drwxr-xr-x  56 amritrosell  staff   1792 Oct  8 21:36 wire_same2
```
END OF HANDOFF w7-transition-prune sha256:5b104c41 21922
