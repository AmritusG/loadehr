```
LOADEHR W9 — THE LOOKS PACK: 14 NEW WAYS TO SHOW / BUILD / CUT / DRESS THE IMPORTED OBJECT (PLEXUS, LIGHT PAINTING, NEON TUBES, 3D SLIT-SCAN, PRINT ON, EXPLODED VIEW, CROSS-SECTION, REVEAL BOX, WINDOW, AUDIO SPIKES, SKIN OF COPIES, CAMERA LOOKS, SHAPE MORPH, BLACK HOLE) — RUN TO FINISHED

MODE: build wave on LoadEhr (this session owns the folder). Five stages IN ORDER (A-E), each built, validated and committed before the next. Commit + push after each stage. Amrit prunes after playing: build every item fully, none is up for "decide whether to keep".
First: cp -p this brief from ~/claude-handoffs/ into docs/handoffs/.

AMRIT'S WORDS (2026-10-08, verbatim; record in the handoff)
"Anything else you can think of that would look amazing in this plugin that mostly handles how the imported objects are handled?"
On Cowork's list of 14: "Lets brief them all and then prune if something doesnt work."
On W8: "Colors look good." (the HQ DXV comparison clips: accepted)

FACTS (verify)
1. Param room: LoadEhr 504, FX 508, Wow 498, Wow FX 512 (FULL). Every new control must be designed to share: mode dropdowns + a few shared dials per group (below). Room is made by moving whole groups between carriers as W2 / W5 did, never by cutting a feature. If no arrangement of four bundles fits, add the fewest bundles (a fifth, e.g. "LoadEhr Lab") and say exactly why; Amrit prunes later and bundles can merge back.
2. Existing machinery to reuse (read it): SampleSurface points (dust / swarm / points), the Swarm's grid-splat neighbour scheme (no compute on GL 4.1), transform feedback, Wire / Points twins + the HDR glow buffer (Wire Color / Glow / Bloom), DEFORM twins, part slots + the per-island data (shatter / pieces), feature-edge flags (W5), clones / instancing, the eye finder openings + glass parts, the projector picture (video incl. DXV), the morph-chain slots B-D, the audio FFT, the floor plane, Trail Echoes (screen feedback) and ghosts, the W14 clock rules, LOADEHR-011 (no mid-shader discard: alpha test at the end).
3. Hard rules: new behaviour in separate program variants (plain programs byte-identical, regress max 0), prewarm at model swap, no geometry / tessellation / compute shaders, 16 samplers per stage, W23 light at alpha 0, W25 never shrink a range, names <= 16 chars, HSBA quartets for any new colour (prefer reusing Wire Color).

READ FIRST (all of it, extract into a design note per stage before coding)
LoadEhr CLAUDE.md in full (W1-W8 sections), ParamList.inc + param_map.py, Shape.cpp, Swarm.cpp, the Wire / Points code (W5 / W6), Post.cpp (bloom, echoes), Projector.cpp, EyeFinder, the transition code (W6 / W7). For each technique also read the standard public description (e.g. plexus / proximity lines, slit-scan / time displacement, clip-plane capping with the stencil buffer, X-ray fresnel shading, LED dot-matrix shading, gravitational-lens screen distortion) and write down the method you use.

STAGE A — LINES (one group "Lines": Line Look OPT Off / Plexus / Light Paint / Neon Tubes + Line Amount, Line Reach, Line Speed, Line Audio; colour / glow / bloom from the Wireframe group)
1. PLEXUS: surface points (Point Source count) joined to neighbours closer than Line Reach by thin glowing lines whose brightness falls with distance; lines follow deforms / spin / shatter; Line Audio pulses reach / brightness with the FFT. Neighbour search without compute: a spatial grid built per frame by splat (as the Swarm) or off-thread on CPU positions for rigid poses; say which and the cost at 5k / 20k points.
2. LIGHT PAINT: N surface points leave ribbons in WORLD space as the model moves / spins / deforms (a ring of their past positions, Line Reach = trail length in seconds, fade by age). Not screen feedback: real 3D trails the camera can orbit.
3. NEON TUBES: the feature edges (W5) as camera-facing glowing tubes (round falloff, hot core) that also LIGHT the model nearby (a cheap line-light term per draw or a light-from-glow-buffer approximation: say which).

STAGE B — TIME AND BUILDING (group "Build": Build Look OPT Off / Slit-Scan / Print On / Exploded + Build Amount (0..1 progress or strength), Build Axis OPT X / Y / Z, Build Speed, Build On Beat OPT)
4. 3D SLIT-SCAN: each slice of the model along Build Axis shows the model's state Build Amount x up to 2 s in the past: keep a history ring of the model's pose / spin / deform / letter state (not pixels), the vertex stage evaluates the past state per vertex height. Smooth (interpolated) across slices, no tearing.
5. PRINT ON: the model builds up layer by layer from the floor (Build Amount = how far printed; Build Speed / On Beat can run it), a glowing printhead line at the build height (Wire Color / Glow), the not-yet-printed part optional as faint wire. Runs backwards to un-print. Proper cap at the cut (see C7).
6. EXPLODED VIEW: parts (part slots, else connected islands) slide out along their direction from the centre by Build Amount, snap back on the beat (Build On Beat, eased); text / logo: per letter.

STAGE C — INSIDE (group "Cut": Cut Look OPT Off / Cross-Section / Reveal Box / Window + Cut Position, Cut Size, Cut Speed)
7. CROSS-SECTION: a plane sweeps through the model (Cut Position / Speed, axis = Build Axis); the cut face is CAPPED (stencil capping of the closed volume; open / non-watertight scans: say how they behave) and glows like a scan (Wire Color, a scan-line pattern); the part beyond the plane hidden or faint wire.
8. REVEAL BOX: the model is solid only inside a moving box / sphere (Cut Size, position drifting with Cut Speed or on the beat); outside it shows as wire or points (the W5 / W6 twins), with a glowing seam where the boundary cuts the surface.
9. WINDOW: the eye openings (eye finder) and glass parts become windows into another world: what shows through them is another model slot (B / C / D) or the projector picture (video / DXV) rendered in its own space behind the openings (stencil the openings, draw the other world there).

STAGE D — SURFACES (group "Surface Look": Surf Look OPT Off / Audio Spikes / Skin Of Copies / X-Ray / Thermal / Night Vision / LED Matrix + Surf Amount, Surf Scale, Surf Speed, Surf Source OPT Slot B / C / D / Text)
10. AUDIO SPIKES: spikes (instanced cones on surface points, along the normal) grow and dance with the FFT bands mapped over the surface (low at the base, high at the top or by region); a ferrofluid feel (spikes cluster, tips glossy). Surf Scale = density, Surf Amount = length.
11. SKIN OF COPIES: hundreds of small copies of another slot (Surf Source) instanced on the main model's surface points, oriented by the normal, scale = Surf Scale, gentle per-copy wobble (Surf Speed); the main model optional (hidden or shown). LOD per copy.
12. CAMERA LOOKS: X-Ray (see-through by viewing angle, fresnel, additive, bones-like edge glow), Thermal (palette by depth / heat / brightness with a thermal colour ramp), Night Vision (green phosphor, grain, bloom, vignette on the object), LED Matrix (the object shown as round LEDs on a grid, Surf Scale = LED pitch, each LED lit by the shaded colour). As object shading modes, not full-frame post, so they combine with the rest.

STAGE E — SHAPE
13. SHAPE MORPH: Spherize generalised: Shape Target OPT Sphere / Cube / Torus / Crystal (faceted octahedron-like) with the existing Spherize amount as the blend; normals follow (DEFORM twin). Spherize's saved values keep working (Sphere is the default target).
14. BLACK HOLE (group "Build" gets it as a 4th Build Look, or its own two dials if room allows): the model spirals into a point (vertices pulled in with swirl, stretched along the fall), and a screen-space gravitational-lens distortion bends what is behind it (the composite and the Arena background behind a FX bundle); Build Amount = how far in; reverse = spits it back out. Light-at-alpha-0 rules hold.

FOR EVERY ITEM
- Zero cost when Off (own program variants / passes, skipped); prewarm at model swap.
- Works on model slots, text and logos; with the morph chain (the dominant model, or both mid-hop: say which); with Deform where it makes sense (say which combine and which do not).
- Slider-driven where it is a progress (scrub back retraces); W14 periods + wraps rows for every clock; headroom past "correct" at the top of each range (the top end is deliberately too much).
- Cost: harness 1080p GPU ms per item at default and at the top end; Arena PERF on the M3 (Wire closed, name the machine).

BUILD + TEST (each stage)
1. Renders per item on skull / helmet / text / logo (strips where it moves), the "too much" end, and one combination with an existing feature (e.g. Plexus + Points, Print On + Wire Only, Cross-Section + Smoke).
2. regress max 0; guards G1-G7 on all bundles; W5 wire, W6 points, W7 transitions strips, W3 / W8 DXV proof, W4 scrub unchanged.
3. Bundle table + counts after each stage (what moved, and why; a fifth bundle only if forced: say why).
4. Install; ask Amrit to quit Arena fully (Arena > Quit Arena, cmd-Q) and relaunch; WAIT; check the PID changed (once per stage that changes the bundles, or once at the end if that is all).
5. CLIPS: one layer per stage ("LoadEhr W9 A Lines", "... B Build", "... C Cut", "... D Surface", "... E Shape"), untriggered, soft ("Soooooft, Crazy as max MAYBE"), one clip per item at a good default + one "too much" clip per stage; selection parked on an empty slot. Say exactly which column holds which clip. Do not touch earlier clips.
6. Commit + push per stage. ONE results handoff at the end: docs/handoffs/loadehr_2026-10-08_w9-looks-pack-results.md, cp -p to ~/claude-handoffs/: per item what it does, its dials, method, cost, what it combines with, the bundle table, clip columns, guards, Commons findings, Amrit's answers from the session.

STANDING RULES
One Claude per folder. Cowork runs no git. Never trigger or activate clips; never kill Arena; do not ask Amrit to save the comp. Carnival mask forbidden. No library, no lasers, no mirror room. W14 clocks, W28 ms host clock, W23 alpha, W25 range clamp, 512 params per bundle, 16-char names, HSBA quartets, Apple GL 4.1 (no geometry / compute / tessellation; LOADEHR-011). Never use AVAssetReader resetForReadingTimeRanges. Licences MIT / BSD / Apache / zlib / ISC / Boost only. Dead code goes. Copies preserve timestamps. Anything Amrit must do: WHAT / HOW / WHERE.

STOP after the handoff. Report.
```
