```
LOADEHR W6 — POINTS ONLY (A POINT-CLOUD LOOK) + A BIG PACK OF NEW MODEL-TO-MODEL TRANSITION STYLES — RUN TO FINISHED

MODE: build wave on LoadEhr (this session owns the folder). Stage 0 then two stages, IN ORDER, each validated and committed before the next. Commit + push at the end of each.
First: cp -p this brief from ~/claude-handoffs/ into docs/handoffs/.

AMRIT'S ANSWERS TO THE W4 / W5 OPEN QUESTIONS (2026-10-08, verbatim; record them in the handoff)
1. Where did ~/Movies/KLV Set go? "Ran out of space, so deleted it." -> intended; nothing to restore. The W3 clips pointing into it are spent test clips: leave them.
2. Should Wire Only default to full strength? "At full."
3. Prune Scrub to DXV / HAP? "Limited."

AMRIT'S WORDS (2026-10-08, verbatim)
"Can we have points only also? And how many transitions types between models can be added?"
Cowork's answer to the second: there is no hard limit. Transition Style is ONE dropdown, so new styles cost no parameter room; the limit is build effort. Under his "add, then prune" rule: build the whole pack below and he cuts after playing.

FACTS (verify in the code)
1. Transition Style today: Dissolve / Shatter / Dust / Slice / Liquid (ParamList.inc P_TRANSITION_STYLE, OPT 0..4). Style Order Fixed / Cycle / Random picks the style per hop (LoadEhrPlugin.cpp ~2075, "round the five": make it round ALL styles). Chain hops, Transition Play, Trans Ease / Stagger / Spread, the W16 rules (slider-driven, scrubbing back retraces, holding still is still, every piece has an order key, pieces at home = the mesh exactly: no pop at either end), the Liquid style's per-slot SDFs, the shatter / dust / pieces machinery, Swarm (Wow), the smoke / fire sim (Wow), Lava (Wow), the Wireframe group (W5) all exist.
2. SampleSurface (Shape.cpp, area-weighted surface points, off-thread) feeds Dust and Swarm.
3. Param room: LoadEhr 501, FX 505, Wow 495, Wow FX 509 of 512. Wow FX has 3 free.
4. Hard rules: new behaviour in separate program variants (plain programs byte-identical, regress max 0), prewarm at model swap, no geometry shaders, 16 samplers per stage, W14 clocks for anything integrated, W23 light at alpha 0.

READ FIRST (all of it; extract before coding)
LoadEhr CLAUDE.md (TRANSITIONS W16, Liquid Stage 4, the morph chain W1, shatter / pieces W2, dust, swarm, sim / fire, lava, Wireframe W5, DEFORM twins, the GL rules, the bundle carriers + 512 cap), ParamList.inc (Model + Wireframe groups), Shape.cpp, Liquid.cpp, the transition code paths in Renderer / LoadEhrPlugin, tools/w16_trans.py, tools/chain_test.py.

STAGE 0 — HIS W4 / W5 ANSWERS (small, first, own commit)
1. "At full": Wireframe Mix default 1 (was 0) and Wire Mode default Off (was Over Surface), so a fresh clip shows no wire until a mode is picked, and any mode picked draws at full strength. W25: moving a default changes no saved clip; say what an old W1 / W5 clip looks like after this (Wire Mode is new since W5, so pre-W5 clips get Off).
2. "Limited": Video Loop Scrub works only for intra-frame codecs (DXV, HAP, HAP Alpha, HAP Q, HAP Q Alpha). For any other codec (H.264, HEVC, ProRes, Motion JPEG) Scrub falls back to Loop and Video Time says "Scrub: DXV / HAP only". Cue-while-playing and reverse speed stay for every codec. Remove the AVFoundation code that existed only for Scrub (the "nearest decoded frame on the way" fallback, scrub-only reader rules), verified inert first; keep what cue / reverse / backward reads still use. Re-run the W4 sweep on DXV / HAP and the cue / reverse tests on H.264.

STAGE 1 — POINTS ONLY
1. Wire Mode gains Points Only and Points + Wire (no new param for the mode). Points take Wire Color, Wire Glow, Wire Bloom / Size, Wire Fade, Wire Back (Hidden = points behind the solid hidden by a depth pre-pass; X-Ray = all); Wire Thickness = point size in pixels (constant on screen); Wireframe Mix = strength.
2. Point Source (OPT, new): Vertices / Feature Vertices (vertices on W5 feature edges) / Surface 25k / 100k / 400k (SampleSurface points, uniform over the area, so sparse models still read). Built off the render thread per slot, cached.
3. Point Twinkle (0..1, new): per-point brightness flicker at whole cycles per loop (W14 period, a wraps row).
4. Points follow Deform, Letters, the shatter flight (each point rides its triangle / piece), clones, the morph chain (both models' points mid-hop). Round, anti-aliased sprites drawn into the same HDR glow path as the wires so Glow / Bloom work identically.
5. Counts: +2 params (Point Source, Point Twinkle) in every bundle: Wow FX 511. If anything else is needed there, move a whole group as W2 / W5 did; never cut.
6. Test: renders per source on skull / helmet / text / logo, coverage numbers, point count shown, cost per source (harness 1080p GPU ms) and Arena PERF on the M3; regress max 0; guards; W5 wire renders unchanged.

STAGE 2 — THE TRANSITION PACK (append to Transition Style; existing 0..4 keep their numbers so saved clips keep their style)
Every new style obeys the W16 rules (slider-driven, scrub back retraces, still is still, order keys, pieces at home = mesh exactly at both ends: measure max 0 at t = 0 and t = 1 vs the plain model), honours Trans Ease / Stagger / Spread where they mean something (say which), works per hop in the chain (A->B, B->C, D->A), and costs nothing when not selected (own program variants). Styles that need a Wow-only system fall back to Dissolve in the classic bundles (say which).
 5  Points: A's surface points fly to B's surface points (matched by sorted order keys), one point cloud morphing into the other.
 6  Shrinkwrap: A's vertices slide onto B's surface (B's SDF gradient), then B fades in on top; reverse for the second half.
 7  Wire: A turns to wireframe (fill dissolves, lines stay), the lines re-form as B's wireframe, B's fill returns.
 8  Voxel: A voxelises (Voxel size grows), the cubes rearrange, B de-voxelises.
 9  Low Poly: A collapses to big facets, facets tumble, B un-facets.
10  Pieces: A breaks into solid pieces (text / logo: the W2 fracture; models: the shatter shards), they fly and reassemble as B's pieces.
11  Implode: A's triangles fall into the centre point, B's burst out of it.
12  Twist: A twists away around the deform axis, B untwists in.
13  Melt: A melts down (Melt / Drip), B rises out of the puddle.
14  Scan: a glowing scan plane (Wire Color, Wire Glow) sweeps across: behind it B, ahead of it A, a wire band where the plane is (the classic "rez-in").
15  Glitch: bands across the axis jump between A and B on the stepped glitch clock until only B remains.
16  Ripple: a wave front from the centre swaps A for B as it passes (Ripple displacement on the front).
17  Swarm (Wow): A dissolves into fireflies, the swarm flies to B's surface and becomes B.
18  Smoke (Wow): A dissolves into sim smoke (Emit Inside), B condenses out of the smoke.
19  Burn (Wow): fire front eats A (blackbody edge, Fire), B is revealed behind it.
20  Lava (Wow): cracks spread over A, A splits apart, B is underneath.
21  Spin Swap: A spins up and shrinks, B spins down and grows (a pop-free 3D swap, the fast "VJ cut").
22  Fold: A mirror-folds into itself (Mirror Fold), unfolds as B.
1. Style Order Cycle / Random go round ALL styles; Random keeps its per-hop hash (stable while scrubbing). Add nothing else to the Model group unless a style truly needs one dial; if it does, fit it inside the 512 cap per the carrier rule.
2. Test per style: a 9-step scrub strip (t = 0..1) on skull -> helmet and text -> logo (classic) / skull -> crown (Wow), forward and backward bit-identical, ends max 0 vs plain models, per-style cost (GPU ms mid-hop), no frame waits on a load, W14 wraps for any clock, 180 min chain Loop over all styles (Style Order Cycle) with no drift.

BUILD + TEST (both stages)
1. Release universal, no warnings in our code; regress max 0; guards G1-G7 on all four bundles; W3 DXV proof, W4 scrub, W5 wire still pass.
2. Install; ask Amrit to quit Arena fully (Arena > Quit Arena, cmd-Q) and relaunch; WAIT; check the PID changed.
3. CLIPS, untriggered, soft ("Soooooft, Crazy as max MAYBE"), selection parked on an empty slot:
   - layer "LoadEhr W6 Points": one clip per Point Source on the skull (cyan, glow), Points + Wire on the helmet, Points Only text with Point Twinkle, points + Shatter On Beat;
   - layer "LoadEhr W6 Transitions": one clip per NEW style (5..22), 2 slots, Transition Play Ping-Pong, Transition Time 6 s, Hold 1.5 s, visually distinct models by path; plus one 4-slot Loop with Style Order Cycle running through all styles.
   Say exactly which column holds which clip. Do not touch earlier clips.
4. Commit + push after each stage. Handoff docs/handoffs/loadehr_2026-10-08_w6-points-transitions-results.md, cp -p to ~/claude-handoffs/: what shipped, style table (number, name, classic / Wow, what Ease / Stagger / Spread do, cost), the end-pop proof, param counts / bundle table, clip columns, guards, Amrit's answers from the session.

STANDING RULES
One Claude per folder. Cowork runs no git. Never trigger or activate clips; never kill Arena; do not ask Amrit to save the comp. Carnival mask forbidden. No library, no lasers, no mirror room. W14 clocks, W28 ms host clock, W23 alpha, W25 range clamp, 512 params, 16-char names, HSBA quartets, Apple GL 4.1. Never use AVAssetReader resetForReadingTimeRanges (W4: heap corruption, Arena hang). Licences MIT / BSD / Apache / zlib / ISC / Boost only. Dead code goes. Copies preserve timestamps. Anything Amrit must do: WHAT / HOW / WHERE.

STOP after the handoff. Report.
```
