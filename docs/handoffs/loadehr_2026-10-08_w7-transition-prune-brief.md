```
LOADEHR W7 — TRANSITION REVIEW: FIX THE TWIST / MELT GLITCH, MUCH MORE TWIST AND MELT, A SPARKLE AT THE CROSSOVER, FIX LAVA ON THE SKULL, REMOVE FOLD + SHRINKWRAP — RUN TO FINISHED

MODE: build wave on LoadEhr (this session owns the folder). DIAGNOSE the two bugs first (measure, find the cause, prove it), then fix; one thing at a time, the glitch first. Commit + push at the end.
First: cp -p this brief from ~/claude-handoffs/ into docs/handoffs/.

AMRIT'S WORDS (2026-10-08, verbatim; record in the handoff)
On W6 open question 1 (InitGL +0.6 s): "Fine."
On the transitions: "Twist and Melt has this glitch. Can the twist be more twisted and the melt be more melted and perhaps a sparkle that kicks in between the models shifting? Lava didn't work on the skull, so it look weird. Fold can go and also Shrink Wrap. Rest stays."

READ FIRST (all of it; extract before coding)
LoadEhr CLAUDE.md (W6 transition pack + style table, W16 rules, DEFORM twins + their normals (W2: finite differences), the Apple GL rules incl. LOADEHR-011 (mid-shader discard miscompile), Lava W26 Stage 7 + W28 "lava holds", the glow layer, W14, the 512 cap: Wow FX is at 511), the W6 results handoff (its note: "onsets at t = 0.01 / 0.99 max <= 75 (DEFORM-twin normals at the first twist / melt)"), the Twist / Melt / Lava transition code, tools/w6_trans.py, tools/arena_w6.py.

STEP 1 — DIAGNOSE THE TWIST / MELT GLITCH (no code change until the cause is proven)
1. Reproduce on Arena's own clips (layer 11 C8 Twist, C9 Melt: their dials) in the harness, real time, both directions of Ping-Pong. Find WHERE in the hop it shows (frame-by-frame difference along t, onset / end / mid, which model) and WHAT it is (shading jump, geometry pop, flicker, a white / black frame, normals flipping, the plain -> DEFORM-twin program switch, the dissolve between A and B).
2. Prime suspect (verify, do not assume): at the hop's ends the draw switches from the plain program (W1 normals) to the DEFORM twin (finite-difference normals) and back; with ~0 deform the twin's normals are not equal to the plain ones, so the shading jumps (W6 measured up to 75 / 255 at t 0.01). Other suspects: Melt's drip clamp, the sim obstacle switching at t 0.5, the Ping-Pong turn, Hold Time edges.
3. Prove the cause with a falsifying test (e.g. force the twin for the whole hop, or force plain normals at deform 0: the glitch must vanish / remain accordingly). Report numbers.

STEP 2 — FIX IT (the proven cause), then show: per-frame change along the whole hop has no spike above the hop's own motion (both directions, both styles, skull + helmet + text), ends still max 0 vs the plain models.

STEP 3 — MORE TWISTED, MORE MELTED (headroom past "correct": the top of each range is deliberately too much)
1. Twist: today Spread = twist amount. Raise the reach so Spread's top end winds the model several full turns (at least 4) round the deform axis at mid-hop, wringing it thin; the twist builds from the ends towards the middle of the hop and unwinds into B. Stagger = which end twists first.
2. Melt: A melts all the way down: sags, drips run (Drip), collapses to a puddle on the floor plane (Floor Height) at mid-hop, B rises out of the puddle and firms up. Spread = how runny / how far the puddle spreads. The puddle must not pop at t 0.5 (A's puddle and B's puddle are the same shape there: cross-fade inside the puddle).
3. Clips show the default AND the top end.

STEP 4 — SPARKLE AT THE CROSSOVER (every style)
1. New dial Trans Sparkle (0..10, default 0, Model group, every bundle): a burst of sparkles on the surface that kicks in as the models shift: rises from t ~0.3, peaks at the swap point of the style (t 0.5 for most; say per style), fades by ~0.7; sparkles sit on both models' surfaces (SampleSurface points), twinkle (W14-periodic), take the surface colour lifted to white, drawn as light into the HDR glow layer at alpha 0 so Wire Glow / Bloom / the global Bloom catch them. Slider-driven like the rest (scrub back retraces, still is still: the twinkle may move, the burst envelope follows t).
2. Param room: Wow FX 511 -> 512 exactly, others +1. If anything else needs a slot, move a whole group per the carrier rule; never cut.
3. Zero cost at 0 (own program / pass, skipped).

STEP 5 — LAVA ON THE SKULL
1. DIAGNOSE first: render the Lava transition (style 20) on Arena's L11 C16 dials, skull -> crown, a 9-step strip + real time; compare with the W26 Lava effect on the same skull (it worked there). Find what "looks weird" (no cracks visible, cracks in the wrong place / scale, Lava Split tearing the scanned skull's many small islands, the crack seed off the camera side, the Lava Light / haze, B not showing under the cracks, the fallback to Dissolve, the scan's thin shell). Prove it.
2. Fix the cause so the skull cracks open into glowing lava and B is revealed underneath, on the skull and on a hard-surface model (helmet) and text.

STEP 6 — REMOVE FOLD AND SHRINKWRAP (they go; not up for evaluation)
1. Delete styles Fold and Shrinkwrap from the Transition Style list and their code, programs, prewarm, data (verified inert after removal; dead code goes). The list keeps its order otherwise; earlier clips are disposable test clips: rebuild only the ones this wave needs.
2. Style Order Cycle / Random go round the remaining 21 styles.

BUILD + TEST
1. W6 end-pop proof re-run on all remaining styles (forward = backward max 0; t 0 / 1 max 0 vs plain); the onset numbers for Twist / Melt now at the level of the other styles (report them all).
2. Cost per changed style + Trans Sparkle (harness 1080p GPU ms mid-hop) and Arena PERF on the M3 (Wire closed; name the machine).
3. regress max 0; guards G1-G7 all bundles; W5 wire renders, W6 points, W3 DXV proof, W4 scrub unchanged.
4. Install; ask Amrit to quit Arena fully (Arena > Quit Arena, cmd-Q) and relaunch; WAIT; check the PID changed.
5. CLIPS, layer "LoadEhr W7", untriggered, soft, selection parked on an empty slot: Twist default / Twist top end; Melt default / Melt top end; Lava skull -> crown (fixed); Lava helmet; Sparkle on Points, on Spin Swap, on Dissolve, and Sparkle 10 ("too much"); a 4-slot Cycle loop over all 21 styles with Sparkle 2. Say exactly which column holds which clip. Do not touch earlier clips.
6. Commit + push. Handoff docs/handoffs/loadehr_2026-10-08_w7-transition-prune-results.md, cp -p to ~/claude-handoffs/: the two diagnoses with proof, the fixes, the new Twist / Melt behaviour, the sparkle envelope per style, removed styles, param counts, costs, clip columns, guards, Amrit's answers.

STANDING RULES
One Claude per folder. Cowork runs no git. Never trigger or activate clips; never kill Arena; do not ask Amrit to save the comp. Carnival mask forbidden. No library, no lasers, no mirror room. W14 clocks, W28 ms host clock, W23 alpha, W25 range clamp, 512 params, 16-char names, HSBA quartets, Apple GL 4.1 (LOADEHR-011: no mid-shader discard). Never use AVAssetReader resetForReadingTimeRanges. Licences MIT / BSD / Apache / zlib / ISC / Boost only. Dead code goes. Copies preserve timestamps. Anything Amrit must do: WHAT / HOW / WHERE.

STOP after the handoff. Report.
```
