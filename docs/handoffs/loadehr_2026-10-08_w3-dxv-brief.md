```
LOADEHR W3 — THE PROJECTOR PLAYS DXV FILES THROUGH DXVKIT (OUR CLEAN-ROOM MIT DECODER), + A VIDEO ALPHA SWITCH — RUN TO FINISHED

MODE: build wave on LoadEhr (this session owns ~/Documents/claude/ffgl/LoadEhr). DXVKit (~/Documents/claude/DXVKit, AmritusG/dxvkit @ cdbb4bc): READ ONLY, consumed as a pinned dependency. Commit + push LoadEhr at the end.
First: cp -p this brief from ~/claude-handoffs/ into docs/handoffs/.

AMRIT'S WORDS (2026-10-08, verbatim)
"But DXV is Resolume native, why can't it be used inside a plugin that is built specifically for Resolume?!?"
"Clean room."
"Are all versions of DXT done properly, alpha and all?"

FACTS (checked; do not re-decide)
1. DXVKit decodes every DXV variant: legacy DXDI (DXT1), DXV3 DXT1, DXT5 (alpha), YCG6 (HQ), YG10 (HQ + alpha). Byte-identical on 127/127 block streams and 91/91 RGBA references, 11/11 invalid files rejected, 320,888 real packets on this Mac decoded with 0 unexplained errors, 30 + 30 min libFuzzer clean. MIT. GPU-path cost (M3 Max, 1 thread): 0.25 ms DXT1 1080p ... 1.77 ms YG10 1080p, 1.18 ms DXT5 4K.
2. DXVKit's GPU output = BCn planes over the 16x16-PADDED texture: DXT1 -> BC1; DXT5 -> BC3; YCG6 -> BC4 (Y) + BC5 (CoCg); YG10 -> BC5 (Y + A) + BC5 (CoCg). Alpha comes out untouched (DXVK_OPT_PREMULTIPLY is opt-in; we do NOT use it: the GPU path never makes RGBA on the CPU).
3. A DXV file does not say whether its alpha is straight or premultiplied (Alley writes straight colour; Adobe's exporter wrote premultiplied-looking colour). Arena makes the user choose per clip (clip properties: Alpha, Premultiplied by default / Straight). So LoadEhr gets the same switch.
4. LoadEhr's video path today (CLAUDE.md "PROJECTOR VIDEO"): AVFoundation codecs + HAP via raw samples -> DXT1 / DXT5 / BC4 uploads, YCoCg converted in kProjVideoFS, the frame drawn into the picture texture premultiplied by its alpha. DXV: "not supported". That line goes.

READ FIRST (all of it; extract into the brief's steps before coding)
1. LoadEhr CLAUDE.md (PROJECTOR VIDEO, Stage 2 projector, the hard-won Apple GL rules, the 512 cap, THE COMMONS: sync + read first). src/Video.h/.mm, src/Projector.cpp, ProjectorShaders.h (kProjVideoFS), the HAP upload path.
2. DXVKit: README, include/dxvkit.h, include/dxvkit.hpp, docs/design.md (plane layouts, padding, jobs callback, status codes, consumption report). Do NOT read DXVKit's spec or test vectors for implementation ideas: use the API.
3. Apple GL 4.1: compressed formats available (S3TC DXT1 / DXT5 via EXT_texture_compression_s3tc, RGTC1 / RGTC2 = BC4 / BC5 core since 3.0): check each with glGetInternalformat / the extension string on this machine, note what you find.

IMPLEMENT
1. DEPENDENCY: DXVKit as a pinned git submodule at third_party/dxvkit (commit cdbb4bc), built by our CMake as a static universal lib (C core only, no Swift). VERSIONS.txt + licence (MIT) noted. Guard: G7 extended to fail on any LGPL file in the tree.
2. VIDEO PATH: Proj File's .mov is opened with DXVKit first (dxvk_mov_open_file); if it is DXV the DXV player runs, else AVFoundation as today. One worker per file, the same decode window / late-frame / Loop / Ping-Pong / Once / Video Speed / Video Restart / clock `clk.video` / trigger behaviour as the other codecs (share the code, do not fork it). Use DXVKit's jobs callback only if it measurably helps (it helps HQ ~1.2x).
3. UPLOAD: the BCn planes as compressed textures (glCompressedTexSubImage2D into textures defined once per size), the padded size; draw into the picture texture through kProjVideoFS with the UV scaled to the display size (crop the padding). YCG6 / YG10: YCoCg -> RGB in the shader (same conversion as Hap Q if the maths matches DXVKit's docs; else its own branch: as a separate program variant if it touches a hot path). No new sampler beyond what the video copy pass already has headroom for (check the 16-per-stage limit).
4. VIDEO ALPHA (new param, Projector group, both Wow bundles): Video Alpha OPT Premultiplied / Straight (default Premultiplied, Arena's default). Straight = premultiply in the copy shader (today's behaviour for every codec); Premultiplied = use as is. Applies to EVERY codec with alpha (DXV DXT5 / YG10, HAP Alpha / Q Alpha, ProRes 4444), so one switch. Counts: Wow 488, Wow FX 502 (<= 512).
5. Remove the "DXV: not supported" line from CLAUDE.md; document the DXV path + the alpha switch; LOADEHR-<NNN> Commons finding for the DXV cost in Arena and one for "DXV alpha mode is not in the file: expose a switch like Arena's".

BUILD + TEST
1. Harness: every DXV variant plays in the projector: the 8 Resolume vectors from DXVKit/test_vectors/streams/resolume (read-only, by path) + real files by path: ~/Movies/Testfiles/Metric_01.mov (legacy), KLV Set/06_THE GREAT SURRENDER (DXT1), black_snowflake_DXV Normal Quality With Alpha (DXT5 4K alpha), KLV Set/01_AWAKE & DISSOLVE (YCG6), Testfiles/YG10-DXV3 (YG10), a 908x2276 portrait one (padding on both axes). Proof per file: the picture texture vs DXVKit's own dump-rgba of the same frame (dxvtool), crop included: mean / max diff (expect 0 or BC-sampling noise only; explain anything else). Odd sizes: no padding stripe visible at the right / bottom edge.
2. Alpha: the DXT5 alpha and YG10 files rendered over the harness background with Video Alpha Straight and Premultiplied: show both; Straight must darken nothing that is opaque.
3. Cost: decode ms / upload ms / late frames per variant at 1080p and 4K (harness), then Arena PERF with one DXV projector clip on the M3 or M5 laptop (name it, Wire closed). 3 min wrap / long-show: Loop over a short DXV clip 180 min, no drift.
4. regress max 0, guards G1-G7 on all four bundles. The W2 files without alpha (H.264, HEVC, ProRes 422, HAP, HAP Q) play bit-identically to W2. The W2 alpha files (HAP Alpha, HAP Q Alpha, ProRes 4444) change at the default on purpose: W2 premultiplied them (= Straight), the new default is Premultiplied (Arena's default). Say so in the handoff and show one of them both ways.
5. Install; ask Amrit to quit Arena fully (Arena > Quit Arena, cmd-Q) and relaunch; WAIT; check the PID changed.
6. CLIPS, a new layer "LoadEhr W3", untriggered, soft, selection parked on an empty slot:
   - one projector clip per DXV variant (DXT1, DXT5 alpha, YCG6, YG10, legacy) on the Human Skull in haze (Wow);
   - the COMPARISON: the same DXT5 alpha file (a) through Proj File with DXVKit and (b) played natively by Arena as the layer's input into Wow FX with Proj Source Input, adjacent columns, same dials;
   - the DXT5 alpha file with Video Alpha Straight and Premultiplied, adjacent.
   Say exactly which column holds which clip. Do not touch the W1 / W2 clips.
7. Commit + push. Handoff docs/handoffs/loadehr_2026-10-08_w3-dxv-results.md, cp -p to ~/claude-handoffs/: what changed, per-variant proof table, alpha strips, costs (harness + Arena, machine named), param counts, clip columns, guards, Amrit's answers from the session.

STANDING RULES
One Claude per folder (this session owns LoadEhr; DXVKit read-only and pinned: any DXVKit bug found is REPORTED, not fixed here). Cowork runs no git. Never trigger or activate clips; never kill Arena; do not ask Amrit to save the comp. Carnival mask forbidden. No library, no lasers, no mirror room. W14 clocks, W28 ms host clock, W23 alpha (API 2.3), W25 range clamp, 512 params, 16-char names, Apple GL 4.1 limits. Licences MIT / BSD / Apache / zlib / ISC / Boost only. Dead code goes. Copies preserve timestamps. Anything Amrit must do: WHAT / HOW / WHERE.

STOP after the handoff. Report.
```
