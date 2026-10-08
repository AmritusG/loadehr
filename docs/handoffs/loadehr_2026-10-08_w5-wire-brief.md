```
LOADEHR W5 — WIREFRAME ONLY (LINES, NO SURFACE), A WIRE COLOUR, WIRE GLOW THAT BLOOMS, IN EVERY BUNDLE — RUN TO FINISHED

MODE: build wave on LoadEhr (this session owns the folder). Runs AFTER W4 (the scrubber) is committed: if W4 is not finished, STOP. Commit + push at the end.
First: cp -p this brief from ~/claude-handoffs/ into docs/handoffs/.

AMRIT'S WORDS (2026-10-08, verbatim)
"While that runs, can I have wireframe surface only? And settings to make the wireframe glow and blom?"

FACTS (read from the code by Cowork; verify)
1. Wireframe today = the FX group's Wireframe Mix (0..1) + Wire Thickness (0.2..8), CLASSIC bundles only (LoadEhr / LoadEhr FX; not Wow / Wow FX). Drawn by the triangle-soup stage; in the main FS (Shaders.h ~l.1506): wire = 1 - min(barycentric smoothstep), outc = mix(outc, brighter outc, wire x Mix), alpha = mix(1, wire, Mix) x surfAlpha. So Mix 1 already fades the fill to alpha 0, but the lines take the SHADED surface colour, the transparent fill still writes depth (back lines hidden), lines are tonemapped (cannot exceed 1, so Bloom barely catches them), and every triangle diagonal shows.
2. Bloom (Post group: Bloom, Bloom Threshold, Bloom Radius) is in every bundle and blooms the whole picture over its threshold.
3. The HDR glow layer (RenderGlow: fireflies ADD, bolts MAX, then added once, light at alpha 0) exists in the Wow bundles.
4. Hard-won GL rules apply: no discard mid-shader (take fwidth first; keep any discard at the very end); never-taken branches in hot code change other renders: new behaviour goes in separate program variants (#define), plain programs byte-identical (regress max 0).
5. Param room after W4: LoadEhr 482, FX 486, Wow 490, Wow FX 504 of 512.

READ FIRST (all of it; extract before coding)
LoadEhr CLAUDE.md (soup stage, the GL rules, Post / bloom, RenderGlow, DEFORM twins, the 512 cap + carrier rules, W14, W23 alpha contract: light at alpha 0), Shaders.h main FS wire block + the soup VS, Post.cpp bloom, the glow layer code, ParamList.inc regions, param_map.py.

IMPLEMENT (full knob set; Amrit prunes after playing)
1. Wire Mode (OPT, replaces Wireframe Mix's role; keep Wireframe Mix as the strength 0..1): Off / Over Surface (today's look) / Wire Only (lines only, no fill at all) / Wire + Dark Fill (lines over a black fill that hides what is behind).
2. Wire Back (OPT): Hidden (the solid's own back lines hidden: depth pre-pass of the fill with colour writes off) / X-Ray (all lines, no depth from the fill).
3. Wire Edges (OPT): All Triangles / Feature Edges (only edges whose two faces bend more than ~20 deg, or open boundary edges: quads read as quads, flat diagonals vanish). Edge flags built off the render thread per model (per-triangle 3-bit mask from shared-edge dihedral angles, cached with the geometry, uploaded as a texture buffer read by the soup stage), for loaded models, text and logos.
4. Wire Color (HSBA quartet = one Arena picker): the line colour; its ALPHA = how much the line takes the picked colour vs the surface colour (0 = surface colour as today).
5. Wire Glow (0..10): the lines' own light, added in HDR before / beside the tonemap so they can go past white and the global Bloom catches them (light at alpha 0 per W23 so it shows over the Arena background).
6. Wire Bloom (0..10) + Wire Bloom Size (0..1): a wire-only halo (the lines drawn into a glow buffer and blurred), so the wires bloom without blooming the rest. Reuse the existing bloom blur / the glow layer where it fits; available in every bundle.
7. Wire Fade (0..1): lines fade with distance from the camera (depth cue).
8. Wire Thickness unchanged (0.2..8 px), anti-aliased as today; it stays constant on screen.
9. EVERY BUNDLE: the wire group moves out of the classic-only FX group into its own "Wireframe" group carried by all four bundles. Wow FX is near full: make room by moving whole groups between carriers as W2 did, never by cutting a feature; report the bundle table and the counts.
10. ZERO-COST OFF: everything new in WIRE program variants (soup FS / VS, depth pre-pass, glow pass); Wire Mode Off = today's programs byte-identical (regress max 0). Prewarm the variants at model swap. Works with Deform, Shatter / pieces (each piece keeps its own edges), Letters, clones, the morph chain (edges of the dominant model), Mirror Floor, projector / lava passes stay as they are (say which combine and which do not).

BUILD + TEST
1. Harness renders per mode x back x edges on the skull, a hard-surface model (Damaged Helmet or a helmet from the test set), text "LOADEHR" and the SVG logo, over the arena background composite; Wire Only shows nothing but lines (fill coverage 0); Feature Edges removes flat diagonals (count lines before / after on a cube-like part).
2. Glow / bloom: Wire Glow 0 / 3 / 10 with global Bloom on: the line luminance passes 1 and blooms; Wire Bloom alone blooms only the lines (the rest of the picture unchanged vs Wire Bloom 0, max 0 outside the halo).
3. Cost (harness 1080p, GPU ms) per mode and with Wire Bloom; Arena PERF on the M3 with one wire clip (Wire closed; name the machine).
4. regress max 0, guards G1-G7, W3 DXV proof, W4 scrub tests still pass.
5. Install; ask Amrit to quit Arena fully (Arena > Quit Arena, cmd-Q) and relaunch; WAIT; check the PID changed.
6. CLIPS, layer "LoadEhr W5", untriggered, soft, selection parked on an empty slot: Wire Only skull (Hidden, Feature Edges, cyan, Wire Glow 3 + Bloom); the same with X-Ray; Wire Only text "LOADEHR" with Wire Bloom; Wire + Dark Fill helmet; Over Surface with Wire Glow (the comparison to today); one Wire Only with Deform Noise Warp + Shatter On Beat (pieces in wire); one "too much" (Wire Glow 10, Wire Bloom 10). Say exactly which column holds which clip.
7. Commit + push. Handoff docs/handoffs/loadehr_2026-10-08_w5-wire-results.md, cp -p to ~/claude-handoffs/: what shipped, the bundle table + counts, renders, costs, clip columns, guards, Amrit's answers from the session.

STANDING RULES
One Claude per folder. Cowork runs no git. Never trigger or activate clips; never kill Arena; do not ask Amrit to save the comp. Carnival mask forbidden. No library, no lasers, no mirror room. W14 clocks, W28 ms host clock, W23 alpha, W25 range clamp, 512 params, 16-char names, HSBA quartets for colours, Apple GL 4.1 (no geometry shaders, 16 samplers per stage). Licences MIT / BSD / Apache / zlib / ISC / Boost only. Dead code goes. Copies preserve timestamps. Anything Amrit must do: WHAT / HOW / WHERE.

STOP after the handoff. Report.
```
