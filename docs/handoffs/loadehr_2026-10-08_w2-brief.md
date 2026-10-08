```
LOADEHR W2 — MIRROR ROOM OUT, TEXT EXTRUSION ON PERMITTED LICENCES, TEXT / LOGO THAT SHATTER INTO REAL SOLID PIECES, A NEW DEFORM GROUP IN EVERY BUNDLE, PROJECTOR PLAYS VIDEO FILES — RUN TO FINISHED

MODE: build wave on LoadEhr (this session owns the folder). MaskEhr: READ ONLY reference. Run the stages IN ORDER, each validated before the next. Commit + push at the end.
First: cp -p this brief from ~/claude-handoffs/ into docs/handoffs/.

AMRIT'S WORDS (2026-10-08, verbatim)
On the W1 open questions: "1: swap. 2: keep. 3: Keep."
"Remove mirror room."
"Can projector use video also?"
"Lets add a deform group so an object (and text and logo) can be deformed by whatever way you see fit."
"Can text and logo be properly shattered?!?"

AMRIT'S ANSWERS (record these in the results handoff)
1. libtess2: SWAP it for Clipper2 (Boost) + earcut (ISC).
2. Model slots: KEEP all four (A-D).
3. Parts group relabelling after the dominant model at each hop: KEEP.

READ FIRST (all of it, extract what you need into the brief's stages before writing code)
1. LoadEhr CLAUDE.md in full, incl. THE COMMONS section (do it first: commons-sync, CONTRACT, INDEX, findings tagged ffgl / arena / text / shatter / deform / video / projector).
2. src/Generate.* (the whole text / logo path: CoreText outlines, nanosvg, CGPDFScanner, the PNG tracer, libtess2 calls, bevel rings, side normals, parts Front / Back / Side / Bevel, triGroup / groupBox, File Colours palette).
3. src/Shape.cpp: BuildShards (the text / logo branch that makes ONE shard per letter and ignores Shard Count), ShardRadii, UpdateBounce, the soup programs (plain, TRANS, BOUNCE, LETTERS, CRACKS variants) and how each reads triShard / shardData.
4. Every vertex stage: main, soup, instanced, shadow, shadow-soup, PROJ_PASS, CRACK_PASS, PHOTON_PASS, liquid capture, dust sampler; where Twist / Bend / Ripple / Melt / Drip / Height Displace are applied today and in which programs.
5. src/Projector.cpp + ProjectorShaders.h (Proj Image decode worker, CPU mips, the cookie texture, Proj Source Input in Wow FX).
6. src/Mirror.cpp, MirrorShaders.h and every #ifdef MIRROR / progXxxM / kMirrorMarchLib / MirrorClip / MirrorCells use.
7. Clipper2 docs (fill rules, PolyTree for holes, Intersect / ClipperOffset) and earcut.hpp README (hole input order, winding). Licence files of both.
8. Apple docs: AVAssetReader / AVAssetReaderTrackOutput (outputSettings nil = compressed samples, BGRA output for decoded codecs), CVPixelBuffer / IOSurface to GL (CGLTexImageIOSurface2D, GL_TEXTURE_RECTANGLE on Apple GL 4.1). Vidvox HAP spec (hap/documentation/HapVideoDRAFT.md) + its BSD reference decoder, Snappy (BSD).

STAGE 1 — REMOVE THE MIRROR ROOM (it goes; not up for evaluation)
1. The Mirror Room group and its params, src/Mirror.cpp, MirrorShaders.h, every MIRROR program variant (main / soup / proj / crack / bounce / every march incl. cookie march), kMirrorMarchLib, kMirrorPointLib, MirrorClip, MirrorCells, the copies' fold matrices, the LED strip pass, mirror prewarm, mirror clocks (LED chase) and their wraps rows, tools/w27_mirror.py, mirror recipes.
2. Anything left with no user after that (dead code goes, verified inert first).
3. Guard G6: no param named Mirror Room / Mirror Count / LED* / Box Size, no Mirror* source. Keep Mirror Floor (Floor group): it is not the mirror room.
4. Parity: every non-mirror look unchanged vs W1 (bit-identical or W1's own run-to-run spread).

STAGE 2 — LIBTESS2 OUT, CLIPPER2 + EARCUT IN
1. Clipper2 (Boost) resolves the fill rule of the source (nonzero / even-odd, from SVG / PDF / CoreText) into outer + hole contours (PolyTree); earcut (ISC) triangulates each outer with its holes; the caps, bevel rings and sides are built from those contours as today.
2. libtess2 removed from third_party, CMake, VERSIONS.txt. Licence files for the two new ones added. Guard G7: no libtess2 / SGI-B in the tree.
3. Test: glyphs with holes and nested holes (O A B 8 % @ and a CJK sample), Font Style Bold / Italic, Letter Spacing, '|' rows, SVG with even-odd and nonzero shapes and a self-intersecting path, the PDF test logo, the traced PNG. Compare W1 vs W2 by silhouette (coverage diff) and by triangle count: the shapes must match (triangulation itself may differ). Show strips in docs/parity/w2_text/.

STAGE 3 — TEXT AND LOGO SHATTER INTO REAL SOLID PIECES
Today a text / logo shatter flies whole letters, and Shard Count is ignored. Make it break the letters themselves into solid chunks.
1. New Shatter Mode (Shape group, OPT): Letters (today's behaviour) / Pieces / Pieces + Letters (pieces fly, each letter's pieces keep a common kick). Default Pieces.
2. Fracture in 2D, then extrude: per letter / logo shape, area-weighted Voronoi seeds inside the outline (count = Shard Count shared over the letters by area, at least 1 per letter), each Voronoi cell intersected with the shape by Clipper2, each piece extruded with the same depth. The original outline keeps its bevel and side; the CUT faces are flat and closed, so every piece is a closed solid with visible thickness.
3. Cut faces are a new part "Core" (part slot after Front / Back / Side / Bevel: own Visible / Hue Shift / Glow / Surface like any part), so the inside can glow (a hot core) or match the outside.
4. At rest and when fully reformed, the UNFRACTURED mesh draws (regress max 0 for text at rest). The fractured mesh is built off the render thread with the spec (cached per spec + Shard Count) and draws only while pieces are away from home. Swap must not pop (pieces at home = the whole shape exactly; same placement, same normals on the outer faces).
5. Everything that uses shards works on the pieces: Shatter / Reform / On Beat, Shatter Force / Gravity / Spin / Reform Time, Shatter Physics Bounce (radius per piece), the Shatter transition style, Lava Split, Letters dials (pieces of a letter ride its letter transform until the shatter).
6. Loaded 3D models: unchanged in this wave (their Voronoi surface patches stay as they are). Say in the handoff, in one line, what it would take to give them solid pieces too.
7. Test: text "LOADEHR" and a two-row text, the SVG and PDF test logos, Shard Count 8 / 64 / 256, each mode, Bounce, the Shatter transition from text (slot A) to a model; renders mid-flight, side + front views; reform ends bit-identical to rest.

STAGE 4 — DEFORM GROUP (new group, carried by EVERY bundle)
Works on any model slot, text and logo. Cowork's knob set under Amrit's "add, then prune" rule: build all of it, he cuts after playing.
1. MOVE into Deform (names unchanged, meaning unchanged): Deform Axis, Twist, Bend, Ripple, Ripple Freq, Ripple Speed, Ripple Beat, Melt, Drip.
2. ADD:
   a. Deform Amount (0..2, default 1): master on every deform. Above 1 is too much on purpose.
   b. Taper (-2..2): scale across the axis grows / shrinks along it.
   c. Squash (-1..1): stretch along the axis, volume kept (squash widens).
   d. Squash Beat (0..2): a squash kick on each beat (continuous envelope, like Beat Bounce).
   e. Shear (-2..2): slide along the axis' first perpendicular.
   f. Bulge (-1..2): push along the normal (negative = pinch in).
   g. Spherize (0..1): blend toward a sphere of the fitted radius.
   h. Noise Warp (0..2), Warp Scale (0.2..10), Warp Speed (0..4): smooth 3D noise displacement (W14-periodic noise, like chrome drift).
   i. Jelly (0..2): the shape lags behind its own spin / move / bounce and springs back (wobble on stops). Integrated per instance, no jumps.
   j. Glitch Shift (0..1), Glitch Bands (2..64): bands across the axis jump sideways on a stepped clock (hash per step, stable while paused).
   k. Mirror Fold (OPT Off / X / X+Y / X+Y+Z): fold the geometry across the fitted centre planes (kaleidoscope object).
   l. Audio > Deform (0..4): the FFT level drives Deform Amount on top.
3. Order: Letters transform -> deforms (fixed order: Taper, Twist, Bend, Squash, Shear, Bulge, Spherize, Noise Warp, Ripple, Melt / Drip, Glitch, Mirror Fold, Jelly) -> shatter / dust / slice flight. Shards and pieces fly from the deformed shape.
4. Normals follow the deform (finite differences along tangent / bitangent or analytic Jacobian), so lighting, rim, env and glass read the new shape.
5. ZERO-COST OFF: all deforms in a `#define DEFORM` program variant of every vertex stage that draws the model (main, soup, shadow, shadow-soup, proj, crack, photon, liquid capture). With every deform at 0 the plain programs draw, byte-identical source (regress max 0). Prewarm the variants at model swap.
6. NOT deformed (say so in CLAUDE.md): the sim obstacle SDF, the eye finder, the liquid SDF (they stay the rigid model), as MaskEhr's Twist / Bend already are.
7. Clocks: Ripple, Noise Warp, Glitch step and Jelly get W14 periods + lines in tools/w14_wraps.py. 180-min test.
8. Units a person can read (Warp Speed = cycles / s, Glitch step = steps / s if a rate dial is added).

STAGE 5 — PROJECTOR PLAYS VIDEO FILES
Answer for Amrit: in Wow FX the projector can already throw the layer's own video (Proj Source = Input). This stage adds video FILES to both bundles that carry the projector.
1. Proj Image becomes Proj File (FILE: png jpg mov mp4 m4v). Proj Source in Wow FX: Input / File. LoadEhr is new: no clip compatibility owed.
2. Decode with AVFoundation on a worker thread: H.264, HEVC, ProRes (incl. 4444 alpha) via AVAssetReader to BGRA IOSurface-backed CVPixelBuffers bound zero-copy (CGLTexImageIOSurface2D); HAP / HAP Alpha / HAP Q via raw compressed samples + Snappy (BSD) -> DXT/BC upload, no CPU decode. DXV: only if Amrit's own Glance decoder code is reachable and its licence is his; otherwise list DXV as not supported.
3. Playback dials (Projector group): Video Speed (0..4, default 1, x real time), Video Loop (OPT Loop / Ping-Pong / Once), Video Restart (EVENT). Position = a double clock wrapped at the clip length (W14); a trigger (WatchTrigger) restarts it like Transition Play. Focus = mip level as today (build mips per frame only if Focus > 0; say the cost).
4. Never block the render thread: frames are queued ahead; a late frame shows the last one; log drops only with Debug Log on.
5. Measure (harness, 1080p and 4K files of each codec): CPU ms per frame, upload ms, memory; Arena PERF with one projector clip on the M3 or M5 laptop (named). Test file: test_media/faces_clip.mp4 + one HAP and one ProRes made with ffmpeg from it (generated, not committed if large).

STAGE 6 — PARAMS AND BUNDLES
1. Deform goes in ALL FOUR bundles. Mirror Room out frees room in the Wow bundles. Make room by moving whole groups between the classic and Wow carriers, never by cutting a feature. Stay at four bundles if any arrangement fits (every bundle <= 512); if none does, add the fewest bundles and say why.
2. Report the groups per bundle as a plain table, the counts, what moved and why. Names <= 16 chars, HSBA quartets for any colour, never shrink a range (W25).

BUILD + TEST
1. Release, universal, no warnings in our code. regress max 0 (refs unchanged: rest renders do not move). Guards G1-G7 on all bundles. Parity vs W1 for every look not touched by the removals or the swap.
2. Install; ask Amrit to quit Arena fully (Arena > Quit Arena, cmd-Q) and relaunch; WAIT; check the PID changed.
3. CLIPS, a new layer "LoadEhr W2", untriggered, soft motion, selection parked on an empty slot. Each with its setting already dialled:
   - Text "LOADEHR" shattering on beat into pieces, Core glowing (Shatter Mode Pieces, Shard Count 64), and the same text in Letters mode next to it (the comparison);
   - the SVG test logo, Pieces + Bounce on the floor;
   - one clip per new deform (Taper, Squash Beat, Shear, Bulge, Spherize, Noise Warp, Jelly, Glitch, Mirror Fold) on one model, + one with everything at Deform Amount 2 (the "too much" end), + one deform on text;
   - projector throwing faces_clip.mp4 onto a model in haze (Wow), and the HAP version next to it.
   Say exactly which column holds which clip. Do NOT repair, re-dial or delete the W1 clips.
4. Commit + push. Handoff docs/handoffs/loadehr_2026-10-08_w2-results.md, cp -p to ~/claude-handoffs/: what went, what was added, the bundle table, the Stage 2 silhouette proof, the shatter design, deform order + cost per deform, video codecs + costs, guards + grep receipts (Mirror, libtess, SGI), clip columns, Amrit's answers (from this brief AND from the CC window).

STANDING RULES
One Claude per folder. Cowork runs no git. Never trigger or activate clips; never kill Arena; do not ask Amrit to save the comp. The carnival mask is forbidden. No library, no lasers. Soft motion ("Soooooft I want, Crazy as max MAYBE"). Dead code goes. W14 clocks, W28 ms host clock, W23 alpha (API 2.3), W25 range clamp, 512 params per bundle, 16-char names, Apple GL 4.1 (no compute / geometry shaders, 16 samplers per stage: check the DEFORM variants stay inside it). Licences MIT / BSD / Apache / zlib / ISC / Boost only. Copies preserve timestamps. Commons findings for every qualifying kind (LOADEHR-<NNN>). Anything Amrit must do: WHAT / HOW / WHERE. If a stage hits a contradiction in the code, STOP that stage, report, carry on with the next independent one.

STOP after the handoff. Report.
```
