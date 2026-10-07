```
LOADEHR W1 — A NEW FFGL PLUGIN FROM MASKEHR: EVERY FEATURE EXCEPT THE LASERS, NO LIBRARY AT ALL, AND 4 MODEL LOADERS THAT MORPH INTO EACH OTHER — RUN TO FINISHED

MODE: new project (spinoff of MaskEhr). MaskEhr: READ ONLY (no edits, no builds inside its tree; reference builds go to the session scratchpad). Commit + push the NEW repo at the end.
First: create the repo, then cp -p this brief into its docs/handoffs/ (it was written in MaskEhr's docs/handoffs/ because Cowork cannot see the new folder yet).

AMRIT'S WORDS (2026-10-07, verbatim)
"I want a version of MaskEhr called LoadEhr that has basically the same features, no lasers though (might prune more) and that does NOT come with any library and does not HAVE a library. Only use two import for morphing between loaded 3D models, or, add more loaders to have more morphing capabilities."

STEP 0 — NAME AND HOME (decided; do not ask)
Name: LoadEhr. Folder ~/Documents/claude/ffgl/LoadEhr/. Private repo AmritusG/loadehr (main). Bundle names LoadEhr (+ LoadEhr FX / LoadEhr Wow / LoadEhr Wow FX only if the param cap forces a split, see IMPLEMENT 5). New 4-char FFGL IDs that collide with nothing in Extra Effects (check). Handoffs: loadehr_<date>_<slug>.md in the repo's docs/handoffs/, cp -p to ~/claude-handoffs/.
Loaders: Cowork's pick under Amrit's "add, then prune" rule = 4 model slots (A, B, C, D). He prunes to 2 later if he wants.

HARD RULES (write them at the top of LoadEhr's CLAUDE.md; each gets a guard test in the regress suite)
1. NO LIBRARY. LoadEhr never scans a folder, never lists models, never ships or bundles a model, never has a default model path. Models come ONLY from the FILE params Amrit points at a file. With nothing loaded it renders transparent and Status says so. (The MaskEhr Library folder stays on disk for MaskEhr; LoadEhr does not know it exists. A clip may load a file that happens to sit in that folder by its path, like any other file.)
2. NO LASERS. Nothing of the Lasers group or the laser code paths survives (that lives in Scotty now).
3. The carnival mask (~/Documents/claude/ffgl/MaskEhr/carnival_mask/) is restricted ("for his use on show only"): never copied into the LoadEhr repo, never committed, never in a test fixture. Arena clips may point at it by path on this Mac only.
Lesson from Scotty W1: "decide with evidence whether to keep X" made the wrong call there. Here nothing in the REMOVE list is up for evaluation. It goes.

READ FIRST (all of it; extract what you need into the new CLAUDE.md)
1. MaskEhr CLAUDE.md in full: the bundle split and 512-param rule, the Layout, Model loading / upload / GpuModel cache, Model B + Transition + Transition Play (W25, W28 incl. the Arena-side re-trigger animation), Liquid morph (Stage 4: FillSdf, SDF per model), Shape slots A/B (src/Shape.cpp), the eye finder (W11/W18) + Light C, SdfBuilder (sim obstacle), Text and logos (W21), orient.txt (W17), the hard-won rules (W14 clocks, W23 alpha, W25 range clamp, W28 ms host clock, Apple GL 4.1 limits, samplers), the W29 PERF instrument.
2. src/ParamList.inc and tools/param_map.py (MEHR_REGION carriers, per-bundle counts).
3. Every place Model B / slot 1 is special-cased (grep Model B, slotB, Library B, transition, liquid, shape slots, SdfBuilder, EyeFinder, part slots) so the A/B design generalises to A-D correctly, not just by copy-paste.
4. docs/smoke_examples.md, docs/wow_examples.md and tools/scripts/*: the recipes to port for parity.
5. MaskEhr's latest handoffs (w28, w29 results) for state and open items.

IMPLEMENT
1. NEW REPO by copying MaskEhr (cp -a / rsync -a, timestamps kept), minus .git, build/, verify/ renders, traces, the stray "-" file, carnival_mask/, and any model files not ours to commit. Own build (universal arm64+x86_64), harness, regress tool (ref-hash guard), param_map, wraps.
2. REMOVE (dead code goes, verified inert, then deleted):
   a. Lasers: the Lasers group, src/Laser.cpp, LaserShaders.h, Laser From Eyes / Rig / Projector, scan patterns, laser dust / dots, laser paths in the mirror room (CPU bounces, beam lattice), the medium hooks that exist only for lasers, laser clocks, MASKEHR_LASER_DEBUG, laser recipes and tools.
   b. Library: src/Library.*, the Library and Library B params and their dynamic lists, folder scan / Reload rescan, category folders, "Not For Shows", "_" folder rules, CREDITS.md / CREDIT.txt handling, any default model path, and the library tools (w19_harvest / sheets / check / download / verify / credits, prune.sh, the Sketchfab code and any MEHR_SKFB use, library orient sheets).
   c. Anything left with no user after a and b (helpers, shader variants, uniforms, params, harness commands).
   KEEP: orient.txt next to a loaded file (it is a per-file side note, not a library), Text / Logo sources, and every other MaskEhr feature (Volume, Smoke Sim, Trip, Fire, Lightning, Swarm, Lava, Caustics, Mirror Room, Lights A / B / C + the eye finder, Spots, Projector, Post, Floor, Silhouette, Shape / Copies / Letters, Camera / Camera Live, FX + Face, Audio, Debug).
3. FOUR MODEL SLOTS: Model A, Model B, Model C, Model D (FILE: fbx obj gltf glb), each with its own Status line and Reload. Model Source (Model File / Text / Logo) applies to slot A as today.
   - Every filled slot loads and uploads in the background ahead of use (MaskEhr's LoadJob / GpuModel / Touch path), so a morph never waits on a load. Empty slots are skipped by the chain.
   - Per-slot data the morphs need is built per slot and cached (liquid SDF, shatter / dust shape slots, eye finder result, sim obstacle SDF).
   - Report GPU memory per slot with 4 heavy models loaded, and the swap-frame cost.
4. MORPH CHAIN (generalises Model B + Transition; keep MaskEhr's names where the meaning holds, so recipes port):
   - Transition (STD 0..1): walks the whole chain of filled slots, A -> B -> C -> D (equal share per hop; integer points = a pure model). Chain Loop (BOOL): D wraps back to A (the last hop morphs D -> A).
   - Transition Style: Dissolve / Shatter / Dust / Slice / Liquid as today, + Style Order: Fixed / Cycle / Random (the style per hop; Random is seeded per hop so it is stable while scrubbing).
   - Transition Play: Off / Once On Trigger / Ping-Pong / Loop over the chain; Transition Time = seconds PER HOP; + Hold Time (seconds resting on each model between hops). Trans Ease per hop. W14 clock rules: double precision, wrapped.
   - The pair being morphed is (current, next); the eye finder / Light C, sim obstacle, part controls and the shadow casters follow the dominant model of the pair (nearer to it), switching at the hop's midpoint without a pop (say how). Part slots: keep MaskEhr's behaviour for slot A and extend whatever Model B does today to C and D; report what Amrit will see in the Parts group as the chain moves.
   - Arena re-trigger: a re-click reaches no plugin (MaskEhr W28). Carry over the Arena-side timeline animation method for the clips (Duration = the whole chain's time).
5. PARAMS AND BUNDLES: build the list, run param_map, and use the FEWEST bundles that carry every kept feature with every bundle <= 512 (lasers + library out frees room; 4 slots + chain controls add some). If a split is still needed, keep MaskEhr's classic / Wow carrier logic and say which groups went where. Names <= 16 chars, HSBA quartets for colours. LoadEhr is new, so no MaskEhr clip compatibility is owed; keep MaskEhr names anyway where meaning is unchanged (Model File becomes Model A: say so).
6. CLAUDE.md for LoadEhr: the hard rules first, then the extracted map (layout, rules, params generated by param_map).

PARITY + TESTS
1. Every non-laser MaskEhr look (the recipe docs + harness scripts) rendered in MaskEhr and LoadEhr with the same model loaded by FILE path (recipes that picked from the Library get that entry's file path), same harness composites (arena, black), shots at 1 / 2 / 3 / 5 s: bit-identical or float noise, diffs shown (side-by-side strips in docs/parity/). Use test models we may commit (test_models/ and our generated ones); the carnival mask may be used only from its original path and only for local renders that are not committed.
2. Laser looks: listed as intentionally gone. Mirror-room looks that had lasers: rendered without them, listed.
3. Morph chain: renders of 2, 3 and 4 filled slots, each style, Loop on / off, Ping-Pong, scrubbing Transition backward; per-hop cost; no frame waits on a load (log). Wraps: all clocks incl. the chain clock pass, no drift at 180 min.
4. Guard tests (in regress): no param named Library* or Laser*; no code path that opens a directory listing for models; no model file inside the built bundle; empty plugin renders transparent.
5. grep receipts in the handoff: Library, library, Laser, laser, Sketchfab, SKFB, CREDIT, Not For Shows, _Pruned: every remaining hit explained or gone.

BUILD + ARENA
1. Release build, universal, no warnings in our code. Install the new bundle(s). Ask Amrit to quit Arena fully (Arena > Quit Arena, cmd-Q) and relaunch; WAIT; check the PID changed.
2. CLIPS (soft, untriggered, selection parked on an empty slot), a new layer "LoadEhr":
   - 3 parity clips next to their MaskEhr originals in the same columns (pick 3 that show the most features: smoke, a Wow showpiece, a transition);
   - 4 MORPH CHAIN clips: (a) 4 slots, Liquid, Loop; (b) 3 slots, Style Order Cycle, Ping-Pong; (c) 2 slots, Shatter, Once (Arena-side re-trigger animation); (d) 4 slots, Dissolve, slow with Hold Time. Load 4 visually distinct models by file path (any file on this Mac; not from "Not For Shows"; the carnival mask may be one of them). Soft timings ("Soooooft, Crazy as max MAYBE").
   Say exactly which slots hold which clip.
3. Commit + push. Handoff: docs/handoffs/loadehr_2026-10-07_w1-results.md, cp -p to ~/claude-handoffs/: what went, what stayed, the slot + chain design, bundle split and param counts, parity proof, memory / cost per slot, grep + guard receipts, clip slots, Amrit's answers from the session (record them, Cowork cannot see the CC window).

STANDING RULES
One Claude per folder: this session owns LoadEhr and only READS MaskEhr. Cowork runs no git. Never trigger clips; never kill Arena; do not ask Amrit to save the comp. Soft motion. Dead code goes. W14 CLOCKS + W28 ms host clock. W23 alpha contract (API 2.3). W25 range clamp. 512 params per bundle. Licences MIT / BSD / Apache / zlib / ISC / Boost only (libtess2 is SGI-B: flag it, it is not on the list; propose a replacement or Amrit's call). Copies preserve timestamps. Anything Amrit must do: WHAT / HOW / WHERE.

STOP after the handoff. Report.
```
