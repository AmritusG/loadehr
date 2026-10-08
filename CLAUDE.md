# LoadEhr — 3D models from files, morphing in a chain: FFGL plugins for Resolume Arena

## HARD RULES (each has a guard in `tools/guards.sh`, run by `tools/regress.sh`)
1. **NO LIBRARY.** LoadEhr never scans a folder, never lists models, never ships or bundles a model, never has a default model
   path. Models come ONLY from the FILE params (Model A..D) pointed at a file. With nothing loaded it renders transparent and
   Status A says so. (MaskEhr's model folder stays on disk for MaskEhr; LoadEhr does not know it exists. A clip or a test may
   point a slot at a file that happens to sit in it, by its path, like any other file.) Guards G1 (no Library* param), G2 (the
   only directory API in src/ = the texture lookup next to the chosen model file, `ModelLoader.cpp FolderIndex`), G3 (no model
   file in a built bundle), G4 (a fresh instance covers 0 pixels, 4 empty slots, Status A says transparent).
2. **NO LASERS.** Nothing of MaskEhr's Lasers group or laser code paths exists here (lasers live in Scotty). Guards G1 (no Laser*
   param), G5 (no Laser* sources).
3. **The carnival mask is FORBIDDEN** (Amrit, 2026-10-08: "Carnival mask is absolutely forbidden to use!"; this overrides the
   W1 brief, which allowed it by path). Never used: not in Arena clips, not in renders (local or committed), not in tests, tools or
   recipes; never copied, never committed. MaskEhr recipes that used it run on MaskEhr's `Masks/Plague Mask/scene.gltf` file in
   LoadEhr (both sides of the parity). `tools/parity.py` refuses any recipe or script that names it.
4. **NO MIRROR ROOM** (W2, Amrit: "Remove mirror room."). Guard G6: no param Mirror Room / Mirror Count / Box Size / Box Depth /
   LED*, no Mirror* source, no MIRROR program variant. (Mirror Floor in the Floor group and Mirror Fold in Deform are not it.)
5. **LICENCES MIT / BSD / Apache / zlib / ISC / Boost only** (W2: libtess2, SGI-B, swapped for Clipper2 + earcut). Guard G7: no
   libtess2 / tesselator / SGI-B in src, third_party, CMakeLists.txt; (W3) no LGPL file anywhere in the tree, submodules included.

LoadEhr is a spinoff of MaskEhr (W1, 2026-10-07, from MaskEhr c7ced33): every MaskEhr feature except the lasers and the library,
plus four model slots that morph into each other. Amrit's words: "I want a version of MaskEhr called LoadEhr that has basically
the same features, no lasers though (might prune more) and that does NOT come with any library and does not HAVE a library. Only
use two import for morphing between loaded 3D models, or, add more loaders to have more morphing capabilities." Four slots =
Cowork's pick under "add, then prune": he may cut to 2. Expect him to prune more after playing with it.

Four bundles from one codebase (`src/LoadEhrPlugin.cpp`, compiled with `LOADEHR_FX=0/1` x `LOADEHR_WOW=0/1`):
- **LoadEhr** (source, ID `LEhr`) — renders any .fbx / .obj / .gltf / .glb over transparent.
- **LoadEhr FX** (effect, ID `LEfx`) — same renderer; the incoming layer can be background, skin or reflection.
- **LoadEhr Wow** (source, `LEwo`) / **LoadEhr Wow FX** (effect, `LEwf`) — the same renderer carrying the W26 showpieces, Trip and
  the projector (+ W2 video files).
IDs checked against every bundle in Extra Effects (2026-10-07: no LEhr / LEfx / LEwo / LEwf anywhere).

BUNDLES: **Resolume shows at most 512 FFGL params per plugin** (Arena drops every param past index 511). One code base, every
param id exists in every build; `MEHR_REGION` blocks in `src/ParamList.inc` say who CARRIES a param: 0 = all, `MEHR_CLASSIC` =
LoadEhr / FX only (Logo, Extrude, Letters, Camera Live, Surface except Glass, Shape except Shatter, Copies, FX, Project Input,
Debug View, W2: Spots, W5: Silhouette), `MEHR_WOW` = Wow / Wow FX only (the W26 groups without Lasers / Mirror Room, Liquid, Physics Speed,
(LoadEhr W1) Face, (W2) Trip, the projector's video dials). W5: Wireframe is in every bundle. W2: Deform (25) and Shatter Mode, Shatter Physics,
Bounciness, Floor Friction are in every bundle. A param a bundle does not carry keeps its default (= off). The host sees the carried params as FFGL indices
0..n-1 in id order (`Map()`; the SDK calls are shimmed by id in `LoadEhrPlugin`, the host entry points convert index -> id).
W1: lasers (-24, Wow side) and the library (-2) went out; four slots (+12: Model / Status / Reload x A..D, replacing Model File,
Status, Reload, Model B) and the chain (+3: Chain Loop, Style Order, Hold Time) came in. That put LoadEhr FX at 521, so Face (10,
FX only) moved to the Wow side. W2: Mirror Room out (-20, Wow), Deform in (+25 everywhere, 9 of them moved from classic Shape),
Shatter Mode (+1), video (+3, Wow). Both FX carriers must fit 2 x shared + both sides' extras <= 1024, so one shared group had to
become single-carrier: Trip (60) went to Wow only; Spots (15) moved Wow -> classic for room; Shatter Physics / Bounciness / Floor
Friction became shared (a logo, classic-only, can bounce). Counts: **LoadEhr 504, FX 508, Wow 498, Wow FX 512** (W3: + Video Alpha; W4: + Video Position, Video Time; W5: Wireframe group
(13) in every bundle, Silhouette (8) moved Wow -> classic for room; W6: + Point Source, Point Twinkle in every bundle; W7: + Trans Sparkle in every bundle: Wow FX at the cap)
(`tools/param_map.py`). Fewest bundles: an effect is a different plugin type than a source, so two carriers per type = four.

Nothing model-specific lives in code. A new model = point a Model slot at a file.

## Build / install / verify
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j     # universal arm64 + x86_64
# install (Arena must be restarted to pick up a NEW plugin; replacing an existing one also needs a restart)
EE="$HOME/Documents/Resolume Arena/Extra Effects"
for b in LoadEhr LoadEhrFX LoadEhrWow LoadEhrWowFX; do rm -rf "${EE:?}/$b.bundle"; cp -Rp build/$b.bundle "$EE/"; codesign --force --sign - "$EE/$b.bundle"; xattr -cr "$EE/$b.bundle"; done
python3 tools/gen_test_models.py          # our test models (knot, mask, crystal, star) -> test_models/gen/ (not committed)
tools/regress.sh <tag>                    # the reference set (max 0 vs verify/w1/ref, ref-hash guarded) + tools/guards.sh
tools/chain_test.py all <out>             # the morph chain: styles x slot counts, play modes, backward scrub, cost, load
tools/parity.py run|report|strips ...     # every non-laser MaskEhr look vs MaskEhr (reference build in a scratch folder)
tools/w14_wraps.py after|times <out>      # every clock's wrap + the 180 min drift test (incl. the chain clock)
PARITY_REF=<w1 build> PARITY_L=<frozen build> tools/parity.py run|report  # W2: parity vs an earlier LoadEhr build
tools/w2_text.py run <out> <ref build>    # W2: text / logo silhouettes + triangle counts vs a reference build (strips: strips)
tools/w2_shatter.py run|strips            # W2: solid pieces (modes, counts, logos, Bounce, transition; reform = rest)
tools/w2_deform.py run|strips             # W2: every deform + its cost
tools/w2_video.py make|run|modes          # W2: projector video files (test media from faces_clip.mp4; HAP via glenc-cli)
tools/arena_w2.py <layer> [col]           # W2: dial the W2 Arena clips by param name (never triggers)
tools/w3_dxv.py proof|alpha|cost <out>    # W3: DXV playback vs dxvtool dump-rgba, the Video Alpha strips, decode / upload cost
tools/w4_scrub.py make|sweep|cue|reverse|latency|longshow|refuse  # W4: the scrubber on frame-coded clips (test_media/gen/w4); W6 refuse: Scrub only on DXV / HAP
tools/arena_w4.py <layer> [col [get]]    # W4: dial / read the W4 scrub clips by param name (never triggers)
tools/w5_wire.py renders|checks|combos|cost <out>  # W5: the wireframe modes, fill / edge / glow / bloom checks, combinations, cost
tools/w6_points.py renders|follow|cost|wire_same <out>  # W6: Points Only / + Wire per source, what they follow, cost, the W5 renders unchanged
tools/w6_trans.py strips|chain|cost|waits|smoke <out>  # W6: the transition pack: scrub strips + ends, D -> A hops, cost, frame waits, Smoke
tools/arena_w6.py points|trans <layer> [col]  # W6: dial the W6 clips by param name (never triggers)
tools/arena_w5.py <layer> [col]          # W5: dial the W5 wire clips by param name (never triggers)
tools/w7_trans.py scrub|spikes|realtime|strip|cost ...  # W7: per-step change along a hop (the swap vs the hop's own motion), Arena's dials played, strips, cost
tools/arena_w7.py <layer> [col]          # W7: dial the W7 clips (Twist / Melt / Lava / Sparkle / Cycle 21) by param name (never triggers)
git submodule update --init               # W3: third_party/dxvkit (DXVKit, pinned: W8 tag v1.0.0) must be checked out before cmake
tools/w8_dxv.py same|forms <out> ...      # W8: pictures byte-identical vs a reference build; the 33 DXVKit Arena forms play / refuse as Arena
tools/arena_w8.py <layer> [col]          # W8: dial the HQ comparison clips (DXVKit vs Arena's own DXV decode) by param name (never triggers)
```
Log: `~/Library/Logs/LoadEhr/LoadEhr.log` (per-material texture resolution, load/upload/swap times per slot, shader compile
logs; with Debug Log: PERF + MEM (GPU MB per slot) lines every second, CHAIN hops). `LOADEHR_LOG_STDERR=1` mirrors it to stderr.
MaskEhr reference builds go to a scratch folder (`cmake -S <MaskEhr> -B <scratch>`), never inside MaskEhr's tree (READ ONLY).

## Model slots and the morph chain (W1)
- SLOTS: Model A, B, C, D (FILE: fbx obj gltf glb), each with its own Status line (TEXT, read-only) and Reload (re-reads that
  file; no folder is scanned). Params in a row: `SlotParam( slot, SF_FILE / SF_STATUS / SF_RELOAD )` (Params.h). Model Source
  (Model File / Text / Logo) applies to slot A as in MaskEhr (Model A = MaskEhr's "Model File"; the option keeps its name).
- Every filled slot loads (LoadJob, worker thread) and uploads (GpuModel, incremental, Touch) in the background whatever the chain
  shows; the process-wide GPU cache shares a file between slots and instances. A slot is FILLED when its file is set and it has
  a model or a load in flight (a failed load with nothing older = empty). Empty slots are skipped by the chain.
- Per-slot caches, built once per slot ahead of use (`Renderer::PrepareSlot` for the filled slots not drawn this frame, by
  what the chain's styles need): shards + bounce data (Shatter), dust points (Dust), the liquid SDF (Liquid), the sim obstacle
  SDF (when the smoke sim runs); in the plugin: the fit glide (Fit To) and the eye search per slot. The renderer draws one PAIR
  per frame (the hop's outgoing model `a`, incoming `b`): `sideSlot[ 2 ]` + `activeSide` pick the cache entry (the same file in
  two slots is ONE GpuModel, so the side, not the pointer, picks it).
- THE CHAIN: the filled slots in order A -> B -> C -> D; H hops (n - 1, or n with Chain Loop: the last morphs back into the
  first). Transition 0..1 walks it, an equal share per hop; a whole point = a pure model (rest = that model as the next hop's
  outgoing model, never the "after the hop" state, so resting on a model looks the same whichever hop reached it).
- Style per hop: Style Order Fixed (Transition Style every hop), Cycle (Transition Style + hop, round the 21 listed styles (W7: Shrinkwrap / Fold retired); W6: while Transition Play loops, + passes x hops, so a 4-slot loop meets every style), Random (a hash
  of the hop index: stable while scrubbing). Trans Ease / Stagger / Spread act inside each hop.
- Transition Play (`ChainPosition`): Off / Once On Trigger / Ping-Pong / Loop; each hop = Transition Time of morph
  (smoothstepped) + Hold Time resting on the model it reached. Once: through the chain, holds the last model. Ping-Pong: to the
  end and back (each arrival rests Hold Time). Loop: with Chain Loop seamless (period H hops); without, the last arrival rests
  max(Hold Time, MaskEhr's min(1.5 s, Time / 4)), then starts again from the first model. 2 slots + Hold 0 = MaskEhr's Transition
  Play. Clock `clk.transPlay` (W14: double, wrapped at its period; Once clamps). The clock holds while the hop it is in lacks a
  model on the GPU; a restart waits for the first hop's incoming model (MaskEhr: until Model B was on the GPU).
- DOMINANT MODEL: the nearer of the pair (outgoing below t 0.5, incoming from it). Without a pop:
  * shadow casters (light shadow maps, the key shadow / floor catcher, the projector's shadow) and the floor mirror draw the WHOLE
    pair mid-hop, each side with its own half of the transition (`DrawCasters`; `progShadowSoupT` = a Shatter hop's shards in
    the shadow passes): they morph with the picture, nothing switches;
  * the sim obstacle switches to the dominant model's SDF (already built per slot) at t 0.5 with NO obstacle velocity that frame
    (last frame's matrix belonged to the other model: p - prev x raw across two fits would kick the smoke): the smoke flows round
    the new shape from then on;
  * the eye openings (Light C, Emit Eyes, eye-to-eye bolts): Place At Eyes / Fit Cone / the auto-place write the C params for the
    ANCHOR model (the first filled slot); every other filled slot gets its own eye search, and the openings move by that model's
    eyes minus the anchor's, GLIDING across the middle of each hop (smoothstep t 0.35 .. 0.65): no jump (`EyeOpen`);
  * fireflies' homes, caustics, lightning strikes (they stay on the model they struck), lava seeds, the Parts labels follow the
    dominant model. Parts: the values act per part SLOT on every model (Part 2 Visible hides part slot 2 of each model, as MaskEhr's
    Model B always did); the labels are relabelled after the dominant model at a hop's midpoint (unused slots hide).
- Clones: the incoming model brings the copies along (`FrameState::instB` = the copies round its own fit), so resting on a model
  shows its clones whichever hop it came from (MaskEhr's Model B never had clones). Ghosts stay with the outgoing model.
- Arena: a clip re-click reaches no plugin (MaskEhr W28), so a re-triggered chain is an Arena-side timeline animation on the
  clip's Transition (Play Once & Clear, Restart away, Duration = the whole chain: H x (Transition Time + Hold Time)), Transition
  Play Off.

## Layout
- `src/ModelLoader.*` — ufbx (fbx/obj) + cgltf (gltf/glb) → `ModelData`; triangulate, normals, Lengyel
  tangents, dedupe, parts sorted by material, auto-fit bbox, texture resolution, stb decode + CPU mips.
- `src/LoadJob.*` — detached worker per file; instances loading the same file version share the job.
- `src/GpuModel.*` — incremental upload (geometry frame, then ≤2 textures/frame, stops after ~4 ms);
  process-wide cache keyed by file version + GL share group; VAOs are per instance.
- `src/Renderer.*`, `src/Shaders.h` — GL 4.1 core. MSAA FBO → resolve → composite into host FBO,
  premultiplied alpha. GGX metal/rough + procedural studio env. Wireframe/Explode use a
  "triangle-soup" vertex stage (index/vertex buffers as R32UI texture buffers) instead of a geometry shader.
- `tools/harness/` — offline host. `third_party/` — FFGL SDK
  (lib only), ufbx, cgltf, stb, meshoptimizer (simplifier only) (all MIT/PD); versions in `third_party/VERSIONS.txt`.
- W20: `src/Sim.cpp`, `src/SimShaders.h`, `src/SdfBuilder.*` (3D smoke sim, see below).
- W21: `src/Generate.*` (text / logos -> a generated model, see below), `third_party/nanosvg` (zlib), W2: `third_party/clipper2`
  (Boost: fill rules, PolyTree) + `third_party/earcut` (ISC: triangulation), replacing libtess2 (SGI-B; Amrit 2026-10-08: "swap"). `tools/probe/TextProbe.cpp` = the W21 text-editor probe (not installed). `test_logos/` = OUR generated test
  emblem (`tools/w21_test_logos.py`: SVG + PDF from one geometry, PNG via sips). `tools/w21_arena.py` = set clip params by name over REST.
- W3: `third_party/dxvkit` (submodule, MIT: DXV decoding), `tools/w3_dxv.py` (proof / alpha / cost of DXV playback).
- W1: `tools/gen_test_models.py` (our test models), `tools/chain_test.py`, `tools/parity.py`, `tools/guards.sh`.
- W6: `src/Points.cpp` (the Points Only / + Wire point sets), `src/Morph.cpp` (the transition pack's MORPH programs, per-pair uniforms,
  the Points / Swarm cloud).
- W3 chain additions: `src/ParamList.inc` (THE param list), `src/Volume.cpp` + `VolumeShaders.h` (Stage 1 shadow maps,
  raymarch, screen rays, motes), `src/Post.cpp` + `PostShaders.h` (Stage 2 key shadow, mirror, silhouette, bloom,
  flares; Stage 5 echoes; Stage 7 face readback), `src/Shape.cpp` (Stage 4 shards, dust transform feedback; per-model
  slots A/B), `src/FaceTracker.*` (Stage 7, FX only, Apple Vision). LODs live in `GpuModel` (built off-thread).
- W11/W18: `src/EyeFinder.*` — Light C's eye finder v2 (CPU orthographic front raster of the Fit-To parts at 512²;
  candidates = see-through holes + depth cavities (closing of the front depth minus depth > 0.04 fitted units; the
  background never lifts the rim); symmetry axis by mirror-mismatch search; pass 1 see-through pairs, pass 2 any pair
  at >= 40 % height; a much larger opening right above a pair rejects it (teeth / grille); no pair -> "visor split"
  (1/3 and 2/3 of the widest band of an opening across the axis). Glass parts are left out (lenses = openings).
  Cavity placements put the light 0.6 x the cavity depth behind the rim (Behind Depth written); the fitted cone is
  capped at 40 deg. Worker thread, cached per geometry + part selection; `LOADEHR_EYE_DEBUG=<png or folder/>`). `tools/param_map.py` regenerates the parameter map below. `tools/probe/` = the W11 colour probe.
- Harness: `composite arena|straight|black` (W23) = how `shot` flattens: arena (default) = premultiplied over a textured
  background, straight = Arena's reading below FFGL API 2.3, black = the old dark checker (regress.txt uses it).
  Params by NAME (`set Spin Y 30`, `_` for spaces in one-token commands), `pulse`, `get`, `compare`,
  `delta` (mean RGB a shot adds over another), `input_seq <dir>`; W21: `text <name> <string>` ("\n" = a line break), `mark` (the next wait_loaded waits for a swap after it); `tools/regress.sh <dir>` re-renders the W1 reference set and diffs it (must stay max 0), then runs the guards. W1:
  `wait_slots <n>` waits for n swaps after a `mark` (the `text` commands after a `mark` keep it).
  `advance <s> [zero]` fast-forwards every plugin clock without rendering (bundle export `LoadEhrTestAdvance`; `zero`
  first sets every phase to 0, W25: and the beat count / puff envelope). Exhale On Beat's SIMAIR is still bimodal run to run
  (0.884 or 0.891, the W24 build the same: verify/w25/exhale_variance.out), like Vortex and Audio Fog. `tools/w14_wraps.py after|times <dir>` = the wrap / long-show test (W14).
  W16: `sweep <steps> <frames/step> <shot prefix|-> <param>` ramps a param 0 -> 1 printing change + coverage per step;
  `hosttime <t>` pins the host clock after loads (with `advance 0 zero`: runs with events / dust / beats are
  bit-reproducible); `LOADEHR_TRANS_SOLO=A|B` draws one side of a mid-hop only. `tools/w16_trans.py` = the
  transition onset measurement (run / report).

## Texture resolution (per material, per channel)
a) texture referenced by the file (embedded bytes → path as written → model folder → `textures/`, by basename)
b) sidecar `<material>_<Channel>.<png|jpg|jpeg|tga|bmp|hdr>` in the model folder or `textures/` (case-insensitive;
   also `<anything>_<material>_<Channel>`). Aliases: BaseColor|Base_Color|Albedo|Diffuse|Color, Normal|NormalGL|
   Normal_OpenGL|nrm, NormalDX|Normal_DirectX (flips green), Roughness|Rough, Metallic|Metalness|Metal,
   Height|Displacement|Disp, Emissive|Emission, AO|AmbientOcclusion|Occlusion, ORM (R=AO G=Rough B=Metal).
   EXR is detected but not decoded (logged).
c) material factors, else neutral defaults. FBX/OBJ: a texture replaces the factor. glTF: factor × texture.
d) W17: KHR_materials_pbrSpecularGlossiness -> diffuse = BaseColor, its spec/gloss map becomes a derived map (R = 1 - gloss,
   G = metallic from the specular level). alphaMode BLEND is drawn as a cutout at 0.5 (no sorted transparency).
   KHR_materials_transmission is logged and drawn opaque. Max Texture Size (Model group: Full / 4K / 2K) drops whole
   mip levels at load (8K Ram Mask: 1024 -> 256 -> 64 MB GPU); it is part of the load / GPU cache key.

## Parts, fit, upload, glass (W18)
- Parts = (node, material) groups: a material used by ONE node stays one part named after the material (MaskEhr's four-material test mask:
  gold / blue / note / mask unchanged); a material spread over several nodes splits per node ("<node>", or
  "<node>/<material>"; exporter names "Object_N" / "<parent>_<mat>_<n>" show the parent's name). > 8 parts: the
  smallest multi-node material is merged back. Draw parts carry a control `slot` (0..7); `slotNames` label the params.
- `parts.txt` next to a model (data, like orient.txt): `Split <part> below <0..1 height> as <name>` moves that part's
  islands reaching below the height into a new part (Flight Helmet: Stand, Plaque).
- Fit To (Model group, All Parts / Visible Parts, default Visible): Auto Fit sizes by the bbox of visible part slots' parts
  (bit-identical to the loader fit when all count); the fit glides 0.5 s (smoothstep, scale in log space). Object-space
  uniforms (uFitCenter/uFitScale) stay the full-model fit so patterns don't swim.
- Upload: textures fill in 2 MB row chunks within ~3 ms per frame (8K worst step 24-31 ms -> ~5-8 ms). PBO streaming
  was tried and stalls worse (14-66 ms): don't. W25: Apple's GL moves a texture's texels to the GPU at its FIRST USE, so the
  swap frame paid for the whole model (8K Corinthian as Model B mid-transition: 44 ms, GPU 36). `GpuModel::Touch` blits 1
  texel out of each texture as it completes (that frame pays for that texture): swap frame 44 -> ~31 ms (8K), 20 -> ~15 ms
  (4K). A touch after every 2 MB chunk was tried: 46-57 ms CPU stalls at each texture start. Don't.
- glTF KHR_materials_transmission (factor) or BLEND with base alpha <= 0.75 (amount 2 x (1 - alpha)) -> the Glass
  surface on that part via per-draw uniforms (shader unchanged), tint = base factor x the map's 1x1 mip; glass parts
  cast no shadows (the eye lights shine through lenses). Part N Surface 0 turns it off.
- W1: every slot loads and uploads in the background whatever the chain shows (the old model keeps drawing until the new one
  swaps); each slot has its own Fit To glide.
- W19: glass parts are drawn in a second pass, blended (premultiplied ONE / ONE_MINUS_SRC_ALPHA) without depth writes, so
  what is behind a lens / cornea / clock glass shows through. No glass = the single pass as before (regress max 0).

## 3D smoke simulation (W20)
- Smoke Engine (Smoke Sim group): Noise = the W3 noise smoke (programs untouched, regress max 0); Sim / Sim + Detail =
  a real fluid in a world-aligned grid (Sim Res 48/64/96/128, box = Sim Box x the mask's fitted size) that follows the
  mask's POSITION (last frame's grid is re-read at +shift). Runs only while the volume runs: a volume light on, or (W23)
  Smoke Ambient / Smoke Key Light > 0 with Smoke Amount > 0. Smoke Ambient = the smoke scatters the key's hue from everywhere;
  Smoke Key Light = the key light lights the smoke with the key shadow map (unit 21; Raymarch only). Both are smoke only, not haze.
- Fields are 2D ATLASES of z-slices (tile z at column z % tilesX): one full-screen triangle per pass (GL 4.1: no compute,
  no image stores, layered rendering needs a geometry shader). `src/Sim.cpp`, `src/SimShaders.h`.
- Fixed substep 1/60 s (max 3 per frame + 8 prewarm substeps per frame for the first 2 s after a start / reset):
  obstacle, MacCormack advect (forward + clamped correction), curl, forces + sources, divergence, Jacobi (Sim
  Iterations), projection. Speed limit 6 cells / substep; smoke / heat clamped at 50 per cell; the grid border is a sink.
- Obstacle = `SdfBuilder`: once per geometry, off-thread, 64^3 in raw model units: surface voxels (triangles sampled at
  half a voxel), solid = everything the outside cannot reach, signed distance by EDT, cavity (G) = the interior once
  holes <= ~6 voxels are closed. Per cell: world -> raw via the inverse model matrix; solid if SDF < half a sim cell;
  obstacle velocity = (p - prevModel * rawPoint) / frame dt (rigid: spin, wobble, bounce, pose). NOT in the obstacle:
  shape deforms (shatter, melt, drip, dust, explode, twist, bend, ripple, voxel, low poly / tumble, height displace),
  hidden parts, the other model of a hop, clones / ghosts: the obstacle is the rigid, undeformed DOMINANT model of the chain (LoadEhr
  W1: one SDF per slot, switched at a hop's midpoint with no obstacle velocity that frame).
- Emitters: Eyes (Light C openings, found by the eye finder when Emit Eyes is on), Behind, Rim (silhouette band of the
  SDF), Inside (SDF cavity). W23 Emit Offset (mask radii) starts Rim and Eyes smoke that far out of the surface. Beat Puff / Audio > Emit multiply all of them. Smoke Color (HSBA; alpha = the sim smoke's
  density) tints the light the sim smoke scatters; Project Input (FX) projects the input copy onto it; Smoke Confine =
  soft wall ~1.3 mask radii out.
- Measured: the collocated grid's central divergence / gradient vs the compact Laplacian leave ~70 % of |div| after
  projection whatever the iteration count; the solver itself converges (residual 2 % -> 0.4 % of |div| for 10 -> 40
  iterations). The consistent 2h-spacing Laplacian was tried: unstable (checkerboard modes). Keep the compact one.
- `LOADEHR_SIM_STATS=1`: every 30 frames a SIMAIR line (W23: share of the smoke > 0.15 mask radii off the surface, plume p90
  height in radii; `tools/w23_air.py air <tag>` = the table for the 12 W22 recipes, reproducible: the SDF lands before the reset,
  clocks zeroed, host time pinned) and a SIMSTAT log line (residual, |div| before / after, speed, total / max smoke,
  smoke inside the obstacle, centroid). GPU readback: debug only.

## Trip (W24): psychedelic looks on the smoke
- Group "Trip" after Smoke Sim. Everything defaults off; Trip Master (0..2, default 1) scales every look (0 = all off; >1 is too
  much on purpose). Time Warp / Freeze are a speed and are NOT scaled by it. `tools/w24_trip.py` = the 14 looks (T1-T14, each a
  W22 recipe + Trip dials), `tools/w24_arena.py` dials them into Arena. W25: `tools/w25_looks.py` (run <tag> [n] = renders of
  W24 / W25 looks; ringprobe = one Fire Ring side + front; satprobe = saturation by brightness), `tools/w25_arena.py` (the 4 W25 clips).
- ZERO-COST OFF: every change lives in `#define TRIP` program variants (march: progFogMarchT / progFogMarchSimT; sim passes:
  progSimCorrectT / ForceT / ProjectT; kTripLib in VolumeShaders.h). They run only while a look is on (`TripState::medium`,
  `SimState::trip`). Measured: even never-taken code in the force pass changed the eye jets' rounding (Dragon SIMAIR 0.906 ->
  0.822): keep the plain passes byte-identical. The TRIP sim passes + sim march prewarm (1-px scissor) whenever the sim starts.
- Sim TRIP fields: smoke b = age mass (density x age; age = b / density, capped 30 s), smoke a = |grad density| per cell (veins),
  velocity a = |curl| (Swirl). Medium looks are Raymarch only; Prism / Hue Echo are a post pass on the fog texture
  (`RenderTripPost`, fog resolution, ping-pong; rgb additive, a coverage: the W23 contract holds).
- Looks: Colour By (Density / Heat / Speed / Swirl / Age / Height; Noise smoke: Height, else its density) x Trip Palette
  (Rainbow / Acid / Neon / Fire / Ice / Picker Pair = Trip A -> B -> A; all periodic in 1) + Palette Shift (turns/s) + Spread;
  Thin Film (n 1.33, chroma only; W25: a soap bubble of 2.5 mask radii seen through each pixel: the phase comes from
  the point projected across the ray + height + a 0.15 density nudge. The W24 film used each sample's radial normal and density:
  ~12 colour cycles along one dense ray averaged to grey. Bright-smoke saturation 0.12 -> 0.41, `w25_looks.py satprobe`); Prism (7 spectral taps along the radial screen direction turned by
  Prism Angle); Kaleido Smoke (N-way mirror fold of the LOOKUP round the mask's front axis, sectors read the one above the mask;
  Twist deg per radius, Spin deg/s); Fractal Fog (a lattice of folded Menger sponges, Fractal Scale mask radii, Iter, Morph deg/s:
  the folds turn); Neon Veins (|grad| / (density + 0.5), Vein picker); Smoke Contours: Contour Style Lines (W25, default) = iso-lines of the
  fog's optical depth -ln(1 - coverage) x Bands x 0.6 in the Trip post pass, anti-aliased by fwidth: Line Width output px, Line
  Glow = halo; Bands (W24) = bands of 1 - exp(-0.8 density) in the volume, palette colours, the density pulled onto the bands; Blacklight (self-emission by density or heat, picker). Emission needs no lamp:
  the volume runs when Smoke Amount > 0 and Veins / Contours / Blacklight are on.
- Sim forces: Fire Ring (EVENT) / Ring On Beat: one torus (eyes: centred between them; mouth; behind = travels up).
  W25 (measured: at Sim Res 96 the core is ~3.6 cells, so the grid was NOT the limit): the W24 ring was driven for 0.15 s at a
  FIXED point, then the grid let the vortex die and confinement / turbulence shredded the core into a band. Now a fired ring
  keeps its world centre + axis and travels (0.6 x Ring Speed, slowing), driven for 2.5 s (full for 0.1 s, then 0.35, fading over
  the last 40 %); smoke only in the first 0.1 s on a tight core (x3); its core is calm (no confinement / turbulence) and holds
  its smoke (cancels Smoke Fade), then dissolves at 3x Smoke Fade. Two rings alive at once (newest first). Smoke Ghosts (per s): a copy of the mask painted in its SDF shell while it
  peels off Ghost Offset radii along the front, growing to Ghost Scale, over Ghost Fade Time s (up to 3 at once). Whirlpool (round
  the mask's up axis), Hyperspace (out from / into the camera axis + toward / away from the camera).
- Time Warp 0..2 = simulated s per s (Noise smoke drifts at the same pace); Freeze (EVENT, held) = 0. A frame with no substep keeps
  the grid where it was (frozen smoke stays put in the world while the mask moves); the next substep applies the whole move.
- W24 also: Sim Turbulence range 0..2 (was 0..5, Amrit; default 0.5; none of the 12 W22 recipes sets it).
- Clocks (W14): Palette Shift wraps at 1 turn, Kaleido Spin and Fractal Morph at 360 deg, Band Scroll at 1 band (the band colour is
  read at (floor(y) - scroll): invariant across the wrap), ghost spawn phase at 1. In tools/w14_wraps.py.

## W26 chain: the WOW effects (one group each, everything off by default; MaskEhr's Stage 1 lasers are not in LoadEhr)
- Tools: `tools/w26_wow.py run <tag> [look]` (THE recipes: LOOKS; renders the Wow bundles over the background), `perf`, `md`.
  Run to run (measured at the bundle split, HEAD and Wow alike): 13 of 21 looks were bit-reproducible; Burning
  Letters, Caustic Glass, Eyes Of Fire, Lightning Eyes, Projection Mapped, Projector Haze are bimodal (like SIMAIR: two or three
  outcomes, each repeats bit-exactly); the two Firefly looks never repeat (transform feedback + additive splat order).
  `src/Wow.cpp` = `WarmWow`, the model-swap prewarm of every W26 program. The plugin's `WowSdf()` holds the slot-0 model's
  SDF job (the same job the sim uploads) for CPU queries: `SdfAt` / `SdfHit` (SdfBuilder, raw model units).
- Stage 2 PROJECTOR (group Projector, `src/Projector.cpp`, `ProjectorShaders.h`): a COOKIE LIGHT. One 1024^2 RGBA32F texture
  in the projector's frustum (aspect = the picture's, Proj Throw = degrees across it): the shadow programs write the distance
  into all four channels, then kProjCookieFS writes the picture into rgb (colour mask RGB; Focus = mip level). So a cookie
  light costs ONE sampler wherever it is read. Picture: FX Proj Source Input (the mip-mapped input copy) or Proj File (png /
  jpg decoded on a worker thread, <= 2048 px, rows flipped bottom-up, CPU box mips; W2: or a video file, see W2 below). Beam = kProjMarchFS added (rgb) onto the
  fog's march target before the temporal blend, the ray clipped to the frustum (5 clip-space planes of its VP) so all Vol
  Quality steps land in the beam; the projector alone forces the volume on with `vol.projOnly` (the main march is skipped:
  cleared target). On the mask = the PROJ_PASS variants of the main programs (`progProjMain / Soup / SoupL`, uPLTex0/1 on
  units 22/23: the main FS has 14 samplers + these 2 = 16), drawn additively over the mask (LEQUAL, polygon offset -1/-4),
  only for the plain draw (no clones / Shatter transition). The plain programs' source is unchanged (#ifdef PROJ_PASS).
  `LOADEHR_PROJ_DEBUG=1` logs the shadow map's coverage. Cost 1080p: +1.7 ms (skull) to +3 ms (a 719k-tri mask: the shadow
  and surface redraws dominate).
- Stage 3 FIRE (group Fire). Reading: Nguyen, Fedkiw, Jensen 2002 "Physically Based Modeling and Animation of Fire": gaseous
  fuel reacts at a thin blue front and becomes hot products; temperature rises across the front then cools radiatively
  (~ c_T ((T - T_air) / (T_max - T_air))^4); buoyancy alpha (T - T_air) up; vorticity confinement keeps the curls; soot is the
  by-product; rendered by blackbody emission of the hot gas. Here, on our grid: the FIRE sim variants (`#define TRIP` +
  `#define FIRE`: progSimCorrectF / ForceF, march progFogMarchSimF; project = progSimProjectT) with smoke b = FUEL. The
  emitters give fuel x Fire (raw smoke x (1 - Fire)) + their heat; fuel burns at Burn Rate once heat > ~0.2-0.8 (smoothstep),
  releasing heat x8 and soot; cooling Fire Cool x T x (1 + 0.05 T^2) (Flame Height lowers it); extra lift capped at heat 3;
  0.8 of the soot's weight cancelled; fuel capped at 8. Measured: full vorticity confinement on burning gas blew the flow
  apart (Vorticity 4, ~2 s in): FIRE halves it. March: blackbody (Helland) at 700 K + (Flame Temp - 700) x tn, tn = 1 - exp(
  -0.3 max(heat - 0.3, 0)), brightness ~ (T / Flame Temp)^4, + a little soot density so the light integrates where the smoke
  is gone; Flame Tint picker (alpha = mix). Fire > 0 switches the sim on (Engine Noise -> Sim). Fire Light = a flickering
  point light appended after the spots (surface only), at the emitters' weighted centre lifted by the flame height: an
  ESTIMATE from the emitters, not a GPU readback. Clock: fire flicker Smooth1D at x9 / x23, wraps at kSmoothPeriod.
- Stage 4 LIQUID MORPH (Transition Style 5th option "Liquid" + Liquid Goo / Look / tint picker / Wobble in the Model group;
  `src/Liquid.cpp`, `LiquidShaders.h`). Both models' SDFs (the shared SdfBuilder jobs) are made liquid-ready OFF the render
  thread: `FillSdf` (morphological closing r 10 voxels: dilate, flood from outside, erode; the border band counts as open,
  the grid margin is only ~6 voxels) so a shell helmet morphs as a solid head, then a separable Gaussian (sigma ~1.2 voxels:
  the raw voxel SDF is stair-stepped). Built as soon as the style is Liquid and both models are loaded (until both are on
  the GPU, the style dissolves like Dissolve). One full-screen raymarch into the main MSAA target with gl_FragDepth:
  d = mix( mix( dA, dB, e ), smin( dA + e S, dB + (1 - e) S, k ), Goo ) - bulge + wobble + shrink; e = smoothstep(0.15,
  0.85, t); S = 0.3 mask radii; the liquid is shrunk inside the meshes near t 0 and 1 while mesh A dissolves into it over
  t 0.03-0.3 and mesh B out of it over 0.7-0.97 (inverted dissolve, kept > 0: an inverted dissolve of 0 shows everything):
  no pop at either end. Look Chrome = a copy of the studio env (tint picker); Paint = both meshes captured shaded from the
  camera at half res, blended by e, gaps filled from rings. Measured: S 0.8 radii shrank the middle to a blob, unfilled
  shells vanished mid-morph, unsmoothed SDF = voxel stairs. Wobble clock: 0.6 cycles/s, wraps at 1. Cost: +0.3 ms (Chrome),
  Paint adds two half-res mesh renders.
- Stage 5 LIGHTNING (group Lightning, `src/Lightning.cpp`, `LightningShaders.h`, bolts built in the plugin). A strike (button,
  beat, or Strike Rate's phase passing 1) makes Bolt Count bolts ONCE from its own seed (the strike counter: discrete, W14-safe)
  by midpoint displacement (Jaggedness) + branches (Branching, two generations, dimmer / thinner). Paths: Crawl = two random
  camera-facing surface points 0.6-1.6 mask units apart, every point Newton-snapped onto the SDF's zero set + 1.2 % of a unit
  out, kept in RAW model units so the bolt rides the moving mask; Eye To Eye = Light C's openings marched out of the SDF along
  the front, arcing forward over the nose; To Floor = the lowest facing point -> the floor plane; To Clones (falls back to
  Crawl without clones). Up to 4 strikes overlap; each flickers over Bolt Life (hash per 1/30 s step). Drawn as instanced
  screen-space quads (24 widths reach, the glow ~0 there) into an RGBA16F bolt buffer with GL_MAX blending (additive glows of
  overlapping segments piled into white blobs), then added (light at alpha 0). Occlusion: eye-space depth with a 6 %-of-a-unit
  tolerance, soft (a hard per-fragment test cut the glow into blocks on curved surfaces). Flash: a point light (surface only)
  at the bolts' centre + the smoke's ambient term (`vol.flash`, uniform values only: the march programs are unchanged); a
  flash with smoke switches the volume on for its duration. Cost ~1.5 ms (mostly that volume march).
- Stage 6 FIREFLY SWARM (group Swarm, `src/Swarm.cpp`, `SwarmShaders.h`). 2k-40k fireflies, transform feedback (as the W6
  dust): homes = area-weighted surface points (the dust sampler, `SampleSurface`, off-thread) + Swarm Offset outward. Boids
  without neighbour lists: each step the swarm is splatted (additive points) into a 32^3 grid round the mask (2D atlas 192^2
  RGBA32F: velocity sum, count): alignment = steer to the cell's mean velocity, cohesion / separation = along the count
  gradient, in when sparse, out when crowded (24 per cell); + curl noise (64-periodic, drift wraps at 64), + a damped spring
  home (Attraction; off for Release Time after a release, back over 1 s), + a soft leash ~2.5 mask units out. Release = one
  outward kick. Drawn into the HDR glow layer (RenderGlow: fireflies ADD, then bolts MAX, then added once, light at alpha 0);
  blink per firefly at whole cycles per kSwarmLoop. Mask Visible off = every part hidden (the swarm IS the mask). FOUND: the
  transform-feedback draw against framebuffer 0 (incomplete in the host's context) fails with 0x506: keep a complete FBO
  bound while the rasterizer is off. Cost ~1 ms.
- Stage 7 LAVA CRACKS (group Lava, `src/Crack.cpp`, kCrackLib in Shaders.h). Cracks = the borders of a jittered 3D Voronoi
  in FITTED object space (Crack Scale x 0.5 cells per fitted unit), measured as the true distance to the bisector planes (two
  3^3 passes, Quilez), behind a front spreading from a seed (Crack Start: a camera-facing surface point picked by the start
  count; Crack Reverse heals; the Crack Spread slider scrubs by hand until the next press; Crack On Beat: W28 cracks
  once, then flares; W26 healed on every other beat). Glow = the CRACK_PASS variants of the main programs (progCrackMain / Soup / SoupC) drawn additively over the mask
  (blackbody of Lava Temp, hottest at the fresh front, a 64-periodic lava flow). Split = the CRACKS soup variants (progSoupC,
  progShadowSoupC; DrawModel swaps them in while Lava Split > 0, plain draws only): each triangle moves with its centroid's
  cell, out from the mask's centre, once the front has passed the cell's site; Lava Light = a volume point light at the
  mask's centre (cube shadow: it only leaves through the gaps; needs a free slot of the two, A / B first). Heat Haze = a
  half-res mask of the FRONT-most crack glow (own depth buffer) read by the HAZE composite variant (progCompositeHaze; the
  plain composite is unchanged), offsets sampled from below too (heat rises); phase wraps at 1. Cost +2.4 ms (cracks + haze),
  +5 ms split with the light in haze.
- Stage 8 GLASS CAUSTICS (group Caustics, `src/Caustic.cpp`, `CausticShaders.h`). Reading: Shah, Konttinen, Pattanaik 2007
  "Caustics Mapping": receivers' positions from the light; the refractor's positions + normals from the light, each texel a
  photon; refract; find the landing point by iterating against the receiver map (estimate d, project, read, update d);
  splat (point sprite) into a light-space caustic map; filter; project it like a shadow / cookie. Here: the caustic light
  sits 5 mask units along the key's direction, 55 deg frustum. Receivers = the opaque mask's distance map (glass parts let
  light through; Caustic Source Whole Mask = no mask receiver) and the catcher plane analytically; refractor = glass parts
  (DrawModel `glassFilter` 1) or the whole mask, PHOTON_PASS variants write position / shading normal at 512^2; photons
  refract ONCE (+ a 64-periodic ripple), 6 receiver iterations, Gaussian splats (energy so flat glass = 1), three IORs for
  Caustic Rainbow. The map = the projector's cookie format (rgb light, a = the receiver distance: mask OR plane, nearer) ->
  the mask (cookie light 2 of the PROJ pass with an EXACT-distance test `uPLExact`: caustics land on their receiver only; a
  shadow-style test lit the glass skull with the floor's caustics), the smoke (`DrawCookieMarch`, the projector's march made
  generic), the catcher plane (the W26 composite variant, `uCausticOn`). The cookie passes' 1 / (1 + d^2) is undone by
  `gain`. Cost +3.4-3.9 ms.
- Stage 9 SHARDS THAT BOUNCE (Shape: Shatter Physics Analytic / Bounce, Bounciness, Floor Friction). On each new shatter
  (`shatterGen`) or a change of the dials, `UpdateBounce` (Shape.cpp) simulates every shard ONCE on the CPU in fitted space:
  the analytic launch (the soup stage's sin-hash on the CPU: same velocity, spin axis and rate), gravity = world down in
  fitted space, the floor plane (Floor Height) + the wall when the catcher is a wall (world planes through this frame's model
  matrix), each shard a sphere of half its radius (`ShardJob::radius`, built with the shards), restitution above 0.3 / s,
  tangential friction that rolls it, damped resting contact; 120 Hz steps, 241 frames (4 s) of offset + quaternion per shard
  in a texture buffer (unit 24). The BOUNCE soup variants (progSoupB / progShadowSoupB, swapped in by DrawModel for plain draws
  while flying; clones and the Shatter transition stay analytic) read it at the flight time, so Reform (time running back)
  retraces the flight; at time 0 the shatter code does not run: measured bit-identical after a full shatter + reform (max 0).
  Cost +0.5 ms.
- (Stage 10 Infinity Mirror Room and its W27 finish: REMOVED in LoadEhr W2, Amrit "Remove mirror room."; HARD RULE 4.)

## W2 (2026-10-08): mirror room out, Clipper2 + earcut, solid text pieces, Deform, projector video
- MIRROR ROOM OUT: Mirror.cpp, MirrorShaders.h, every MIRROR program variant (main / soup / proj / crack / bounce / the five marches /
  cookie march / bolts / swarm / motes), the LED pass, its prewarm, BuildMirror, the LED chase clock, tools/w27_mirror.py, the w26
  Stage 10 recipes. `Instance::bounces` went; uInstX.w is unused (0). Parity vs W1 (66 -> 64 looks, PARITY_REF): 0 DIFFERS.
- TEXT / LOGO OUTLINES: `Boundary()` = Clipper2 Union under the source's fill rule (NonZero / EvenOdd; Positive for the Outline
  Only ring and the caps) -> PolyTree -> contours, outers CCW, holes CW (region on the left, as libtess2 gave); `Triangulate()`
  = each PolyTree outer + its holes -> earcut (islands in holes recurse). Precision: ~9 significant digits of the largest coordinate.
  Proof (tools/w2_text.py, docs/parity/w2_text): 15 cases (holes O A B 8 % @, Bold, Italic, Letter Spacing, '|' rows, CJK, a font
  file, our even-odd / nonzero / nested / self-crossing SVGs, the emblem SVG / PDF / traced PNG): silhouettes max 0 vs W1, triangle
  counts equal except CJK (20958 -> 20840); lit 3/4 views bit-identical except z-fighting between two coplanar overlapping shapes.
- SOLID PIECES (Shape: Shatter Mode Letters / Pieces / Pieces + Letters, default Pieces). With Pieces the spec carries
  `frac=<Shard Count>` and the generator builds a FRACTURED COPY next to the whole mesh (`Fracture()` in Generate.cpp): per letter,
  seeds on its front cap (best of 12 candidates, count shared by front area, >= 1 per letter), 2D Voronoi cells (convex, bisector
  half-planes); every finished triangle (caps, bevels, side) is Sutherland-Hodgman clipped by each cell in barycentric space
  (attributes interpolated), so a piece's outer faces ARE the letter's surface; each cell edge through the solid gets a flat cut face
  between the top and bottom section lines read off the clipped triangles' edges on it (no back cap: down to the back plane): the
  part "Core" (slot after Bevel; on the whole mesh an empty part, so the label exists at rest). The copy is finished on its own
  (no vertex merges into the whole mesh) and appended to the same buffers: parts flagged `fractured` (same control slots),
  `ModelData::wholeIndexCount`, triPiece. CpuGeometry idx / parts = the WHOLE mesh only (dust, SDFs, LODs, surface points unchanged);
  fracIdx / fracPiece / pieceCount carry the copy; triGroup covers both (the letters move the pieces). DrawModel draws the copy
  instead of the whole mesh only while pieces fly (soup, shape on, shatterT > 0, mode > 0) -> at rest and once reformed the whole
  mesh: bit-identical (tools/w2_shatter.py: 12 cases + the transition, reform = rest max 0). Shards: ids 0..L-1 letters, L.. pieces
  (Pieces + Letters: a piece takes its letter's seed = one kick / spin per letter); ShardKey = serial, count, mode. Bounce, the
  Shatter transition, Lava Split, Letters work on them unchanged (same soup programs). LOADEHR 64: 65 pieces, 17.5k tris, built in
  ~10-30 ms with the spec. Loaded models: unchanged (Voronoi surface patches).
- DEFORM group (every bundle; `src/ParamList.inc`): Deform Amount (x every deform, + Audio > Deform x low band), Axis, Taper, Twist,
  Bend, Squash (+ Squash Beat: the beat envelope), Shear, Bulge (rounds the axis' middle out: radial, crack-free; NOT along vertex
  normals, which tear text open at its hard edges), Spherize (to 0.8 fitted), Noise Warp / Scale / Speed (64-periodic value noise,
  drift x:y:z = 1 : 0.5 : 0.25 cells, wraps at 256), Ripple x4, Melt / Drip, Glitch Shift / Bands / Rate (steps / s; hash per band
  and step, wraps at 1000 steps), Mirror Fold Off / X / X+Y / X+Y+Z, Jelly. Fixed order in `DeformPre` / `DeformPost` (kSoupVS).
  ZERO-COST OFF: every program linked from kSoupVS gets a DEFORM twin at Init (Link() spots the soup VS; 21 twins; sampler units
  copied from the plain one); DrawModel swaps the twin in when `shape.deform` (any deform non-zero) -> the plain programs draw
  otherwise (regress max 0; the plain source differs from W1 only inside #ifdef DEFORM and the INST_ID macro). The plain soup's
  twist / bend / ripple / melt code is unchanged, but those dials now always draw through the twin (smooth deformed normals instead
  of W1's flat facets: the only W1 parity change in w6_shape). NORMALS: finite differences of the chain along the vertex tangent /
  bitangent (0.01 fitted), Glitch left out (a band's jump is a translation). MIRROR FOLD: every draw becomes 2 / 4 / 8 instanced
  copies (`uFoldN`, `INST_ID = gl_InstanceID / uFoldN` for clones), copy c keeps its own side of each folded centre plane; a
  triangle reflected an odd number of times swaps two corners (winding stays front-facing) and flips its FD normal; Deform Amount
  folds it gradually. JELLY: a damped spring per instance (1.6 Hz, damping 0.22, 240 Hz steps, clamped) driven by the mask's own
  linear / angular / scale acceleration in mask space (`StepJelly`); the shader offsets, rotates and scales by it x min(|f|, 1.5).
  NOT deformed: the sim obstacle SDF, the eye finder, the liquid SDF, the dust points (they stay the rigid model). Prewarm: the
  main twins at model swap, the shadow twins in WarmVolume. No new samplers. Cost (harness, 1080p, Human Skull 31k tris, GPU ms
  avg vs every deform 0): each deform -0.2..+0.5 (noise), Mirror Fold X+Y+Z +1.6, everything at Amount 2 +1.3 (tools/w2_deform.py).
  Clocks: Noise Warp and Glitch rows in tools/w14_wraps.py (ok); the 'deform' scene flat over 180 min.
- PROJECTOR VIDEO (Wow bundles): Proj File = png / jpg or mov / mp4 / m4v (`src/Video.h/.mm`). One VideoPlayer per file, its own
  worker: AVAssetReader; H.264 / HEVC / ProRes (4444 alpha too) -> BGRA IOSurface-backed CVPixelBuffers, bound zero-copy
  (CGLTexImageIOSurface2D on a GL_TEXTURE_RECTANGLE); HAP / HAP Alpha / HAP Q / HAP Q Alpha / HAP Alpha-Only = raw samples
  (outputSettings nil; skip zero-sample marker buffers), sections parsed + Snappy (our own decoder, from the format docs) -> DXT1 /
  DXT5 / BC4 uploads, Hap Q's scaled YCoCg converted in the copy shader. HAP R / HDR (BC7 / BC6): refused (no BPTC here). DXV (W3):
  see below. The frame is drawn into the picture texture (<= 2048 px) — kProjVideoFS; mips only while Focus > 0. Decode window: the play position + 6 frames in its direction (Loop
  wraps it); a backward window is read forward from a few frames before; the render thread never waits (a frame not decoded yet
  -> the nearest one behind, else the last one: counted "late"). Dials: Video Speed (x real time), Video Loop (Loop / Ping-Pong /
  Once / Scrub), Video Position, Video Time, Video Restart; clock `clk.video` (Loop wraps at the clip length, Ping-Pong at twice it,
  Once clamps); a clip trigger restarts it. W4: see VIDEO SCRUBBER below. Debug Log: one VIDEO line per second (decoded, ms each, late, upload ms, cache MB). Measured: tools/w2_video.py
  (13 files, harness) and Arena (MBP M3 Max): see the W2 results handoff.
- DXV (W3, `RunDxv` in Video.mm): DXVKit (`third_party/dxvkit`, a git submodule pinned at tag v1.0.0 = 2f5256a since W8, our clean-room MIT decoder, built
  by our CMake as a static universal C lib, no tools / Swift) opens EVERY Proj File video first (`dxvk_mov_open_file`); a DXV track
  plays here, anything else falls back to AVFoundation. Every packet is a whole frame: the worker reads + decodes the wanted frame
  directly (no reader state) into DXVKit's block planes over the 16-PADDED coded texture; the decode window / late frames / Loop /
  Ping-Pong / Once / Speed / Restart / `clk.video` are the shared code (`Window()`). Uploads (each texture defined once per size +
  format, then glCompressedTexSubImage2D): DXT1 / legacy DXT1 -> 0x83F0 (opaque BC1), DXT5 / legacy DXT5 -> 0x83F3, YCG6 -> RGTC1 Y +
  RGTC2 Co, Cg at half size, YG10 -> RGTC2 Y, A + RGTC2 Co, Cg; on the copy pass's existing uDxt / uDxtA samplers (no new sampler);
  uDxtFit = display / coded crops the padding. W8 (DXVKit 1.0.0, Arena 7.28's convention, spec 8.4): plain YCoCg with chroma midpoint
  0.5 (127.5 / 255, not 128 / 255) and R / B overflow folded into the other channel before the clamp (kProjVideoFS modes 5 / 6; Hap Q's
  branch untouched, byte-identical). W8 acceptance: DXVK_GEN_AUTO (the packet decides legacy vs DXV3); frame 0 is decoded at open and a
  file whose frame 0 fails (DXVK_ERR_BAD_HEADER: DXV3 version < 4, stored flag > 1; Arena plays those black) is REFUSED; a later frame
  that fails is skipped (marked, never fetched again; the last frame stays; one VIDEO line with Debug Log). A refused video file keeps
  the projector's last picture (`ProjState::videoKeep`). An AVFoundation track that is not `decodable` (FourCC DXV3 / DXT1 / YCG6 / DXD1
  / DXDA: Arena "not a clip") is refused at open (it used to read nothing forever, LOADEHR-017). Proof (tools/w3_dxv.py proof vs
  dxvtool 1.0.0): HQ max 1-3 (was 14-84 with midpoint 128), DXT max 0 vs W7; the 33 Arena forms 33 / 33 (tools/w8_dxv.py forms). Chroma is sampled NEAREST
  (the DXV spec's chroma): linear smeared colour edges (testsrc2 max 107 of 255) and pulled the padding's chroma into the last row
  (97). GL (M3 Max): GL_COMPRESSED_TEXTURE_FORMATS lists only the three S3TC formats, but RGTC1 / RGTC2 upload without error (core
  3.0); BPTC: INVALID_ENUM. Proof (`tools/w3_dxv.py proof`, LOADEHR_VIDEO_DUMP=<folder/> = each new picture as raw RGBA, debug):
  the picture vs `dxvtool dump-rgba` differs only by the GPU's BCn palette precision (DXVKit's palette truncates as the spec says;
  a float-exact CPU decode of the same blocks matches the picture on 98.6 % of values, the rest +-1).
- VIDEO ALPHA (W3, Projector, Wow bundles): a file does not say whether its alpha is straight or premultiplied (Alley writes straight,
  Adobe's exporter premultiplied-looking colour); Arena asks per clip, Premultiplied by default. Video Alpha Premultiplied (default) =
  the colour as it is; Straight = colour x alpha in kProjVideoFS (`uStraight`; what W2 did for every codec). Every codec with alpha
  (DXV DXT5 / YG10, HAP Alpha / Q Alpha, ProRes 4444). HAP alpha-only = the alpha itself either way. Changing it redraws the held frame.

## W4 (2026-10-08): the projector video's scrubber (Wow bundles)
- WHY OUR OWN PARAM: Arena's transport bar does not exist for source / FFGL clips and its position never reaches a plugin
  (FF_SET_TIME is a global ms clock, W28; clip_position is refused as a phase source for FFGL params). So the transport is Video
  Position + Arena's own param tools (drag, MIDI / OSC map, the param's timeline animation = Play Once / Loop / BPM sync).
- W6 (Amrit: Scrub "Limited."): Video Loop Scrub only on codecs whose every frame decodes on its own: DXV, HAP / HAP Alpha / HAP Q /
  HAP Q Alpha / HAP Alpha-Only (`VideoPlayer::Intra`). Any other codec (H.264, HEVC, ProRes, Motion JPEG) plays Scrub as Loop (a
  Video Position change cues) and Video Time starts "Scrub: DXV / HAP only". Cue and reverse speed stay for every codec. The scrub
  hold's "nearest decoded frame on the way" was meant for long-GOP drags but still fires on HAP / HAP Q 4K drags (W6 sweep: 25 / 63
  pictures on the way), so it stays; the backward start rule (no earlier than the key frame) serves a cue's hold too, so it stays.
- Video Loop Scrub: no clock; the frame = round(Video Position x (N - 1)) exactly; `clk.video` waits on it (back to a playing mode =
  plays on from there). Until the frame is decoded the last one shown stays (never black). Loop / Ping-Pong / Once: a USER change of
  Video Position CUES: the clock jumps to that frame and waits on it until it is on screen (max 1 s), then plays on. Measured
  (tools/w4_scrub.py, frame-coded clips, 1080p + 4K): sweeps / cues / reverse exact on DXV, HAP, HAP Q, ProRes, H.264, HEVC (H.264 4K
  with ONE key frame cannot follow a 0.25 s backward step: ~110-390 ms per seek). Jump latency (request -> on screen, harness 60 Hz):
  DXV / HAP 1 render (~20 ms), ProRes / HAP 4K 2-4, H.264 / HEVC 1080 3-4 (65-78 ms median), 4K one-key ~110 ms; drags 0 ms (prefetched). PLAYHEAD: while
  playing the plugin writes its frame position into Video Position + FF_EVENT_FLAG_VALUE, at most 15 / s; a host set equal to one of
  the last 8 pushed values is Arena echoing ours (SCOTTY-005), not a cue. A restart pending in the same frame as a cue wins (a clip
  load's saved position is no cue). Video Time (TEXT, read-only like Status): "mm:ss.f / mm:ss.f  frame n / N  codec", 5 / s.
- Video Speed -4..4 (W25-safe widening). Loop / Ping-Pong wrap as before (Wrap handles < 0); Once clamps to 0..len and a restart at
  a negative speed starts from the end (plays back once, holds frame 0). The decoder's direction = sign(speed) x the Ping-Pong half.
- SEEKS: DXV = the wanted packet decoded directly (random access). AVFoundation (H.264 / HEVC / ProRes / HAP): an AVSampleCursor
  pass at open lists the key frames; the open reader keeps reading while that delivers fewer frames than a new reader (key frame ->
  frame + 16: a new reader costs cancel ~15 ms + start ~15 + first sample ~9, measured W4); else a new reader there. Each sample
  re-reads the request (the latest position wins, fast drags coalesce). BACKWARD (play or scrub): the window keeps the near 6 frames
  + a chunk below them (~256 MB / up to 30 frames; HAP 24); a chunk is read (forward, from a new reader) only when a frame within 6 of
  the play position is missing, so every frame it delivers is kept (a window of only `behind` frames re-read ~31 frames to keep 2 and
  fell behind). Scrub backward reads max(6, key frame -> frame) frames (all-intra: no reader per frame on a drag back). Scrub hold:
  the last frame, and from the 3rd render waiting the decoded frame nearest the target on the way from it (a fast long-GOP drag moves
  on instead of freezing); a cue's hold is strict (no in-between frame). A 1-frame clip (wrap puts frame 0 behind AND ahead) decodes.
- FOUND (W4, LOADEHR-008): AVAssetReader hands out the frame covering the range start stamped WITH the range start; the W2 open at
  (start - 0.5) / fps showed every backward read one frame early. Opened at (start + 0.25) / fps now.
- DEAD END (LOADEHR-009): AVPlayer + AVPlayerItemVideoOutput seeks 1.1-3x faster than a new reader but needs the MAIN run loop (from
  a worker with none: never ReadyToPlay in 3 s). One random-access reader re-aimed per seek (`supportsRandomAccess` +
  `resetForReadingTimeRanges`) was ~10x faster per jump (ProRes 1.8 ms) BUT corrupted MediaToolbox's heap (harness: 3 crashes,
  malloc freelist / "pointer being freed was not allocated" under resetForReadingTimeRanges) and left a worker deadlocked in
  copyNextSampleBuffer in Arena (Arena hung; Amrit force-quit it). Removed. AVAssetImageGenerator ~= a new reader + a CPU copy.
- Debug Log: SCRUB lines (frame, ms from the request to the frame returned, renders held meanwhile; superseded targets) once a
  second. LOADEHR_VIDEO_DUMP_SEQ=1 (with LOADEHR_VIDEO_DUMP) names the dumps <seq>_<frame>_<w>x<h>.rgba (the order shown).

## W5 (2026-10-08): wireframe only, a wire colour, wire glow that blooms (every bundle; Amrit: "wireframe surface only? And settings to make the wireframe glow and blom?")
- GROUP "Wireframe" in all four bundles (Wireframe Mix / Wire Thickness moved out of classic FX, names kept): Wire Mode Off / Over
  Surface / Wire Only / Wire + Dark Fill; Wireframe Mix = the lines' strength in every mode (0 = no lines: Wire Only at Mix 0 shows
  nothing). W6 (Amrit: "At full."): defaults Wire Mode Off, Wireframe Mix 1 (a fresh clip: no wire; any mode picked: full strength).
  A pre-W5 clip (no Wire Mode saved) now opens with Wire Mode Off: its W1 wire (Wireframe Mix > 0) is gone until Wire Mode is set to
  Over Surface; a W5 clip keeps its saved mode and mix (W25: a moved default changes no saved value); Wire Back Hidden / X-Ray; Wire Edges All Triangles / Feature Edges; Wire Color
  (HSBA picker; its alpha "Wire Color Mix" = picked colour vs the surface colour); Wire Glow; Wire Bloom + Wire Bloom Size; Wire Fade.
- ZERO-COST OFF: Over Surface with Color Mix / Glow / Bloom / Fade 0, Back Hidden, Edges All = `fs.wireMix` on the PLAIN programs (the
  W1 path; regress r2_wireframe50 max 0). Anything else sets `fs.wire.on` and the main draws go through the WIRE twins: Link() gives
  every soup program that links the MAIN fragment stage (not SHADOW / CRACK / PROJ / PHOTON passes) a `#define WIRE` twin, and each
  WIRE twin its own DEFORM twin (8 + 8; prewarmed at model swap with the DEFORM twins). `DrawModel` swaps the twin in for soup draws
  while `fs.wire.on` (programs without a twin stay themselves); `SetWireUniforms` binds the edge flags (unit 25).
- DRAWS (`DrawWired`, Wire.cpp; blended ONE / ONE_MINUS_SRC_ALPHA, uniform uWirePass): Over Surface Hidden = one draw (the W1 formula
  with the line colour); Wire Only Hidden = the solid's depth (pass 0: alpha 1 for the discard test, colour 0) then the lines (pass 2,
  LEQUAL, no depth write: same program and uniforms, so the same depth); Dark Fill Hidden = one opaque black draw with the lines;
  X-Ray = the surface / fill (Mix 0) or nothing, then the lines with no depth test. Clones (instanced; a simplified LOD draws every edge),
  ghosts, the morph pair (each side its own draws) go the same way. Wire Only skips the projector picture and the lava cracks (nothing
  to land on); the Mirror Floor, the shadow casters and the other passes draw the SURFACE (fs.wireMix = 0 there).
- FEATURE EDGES: per model, a worker (`BuildEdgeFlags`): vertices welded by position, edge key = welded pair + region (the whole mesh;
  the fractured copy per piece, so each piece keeps its own edges); an edge draws if open, non-manifold, or its faces bend > 20 deg;
  bit k = the edge opposite corner k (the soup barycentric k). R8UI texture buffer, read per triangle (base / 3) in the WIRE soup
  stage; until it lands every edge draws. Cube: 24 of 36 triangle edges (the 6 diagonals out). Skull 31k tris: 4 ms.
- GLOW / BLOOM: the main pass output is display-space (tonemapped in the FS, RGBA8 targets, postTex RGBA8), so "past white" is an
  HDR WIRE BUFFER (RGBA16F + own depth): the wired draws again (`wireDraws`), the solid's depth first (Hidden), then the lines (pass 4:
  lineColour x coverage as light). Wire Bloom = that buffer at half size, `progBlur1D` across then down (taps 4 + 20 x Size, stride
  (1 + 3 x Size) x h / 1080). `AddWireLight` adds glow x 0.35 x Wire Glow + halo x 0.6 x Wire Bloom over the composite at alpha 0
  (W23 light) BEFORE the post pass, so the global Bloom catches the lines too. Runs only while Wire Glow or Wire Bloom > 0.

## W6 (2026-10-08): points only, and a pack of model-to-model transition styles (every bundle)
- STAGE 1 POINTS (Amrit: "Can we have points only also?"): Wire Mode + Points Only (4) / Points + Wire (5); Point Source (Vertices /
  Feature Vertices / Surface 25k / 100k / 400k, default 100k), Point Twinkle (0..1) in the Wireframe group. A point = a spot on a
  triangle (global triangle in GPU index order, barycentric b1 b2; RGBA32F texture buffer, unit 26, `src/Points.cpp`). The POINTS twins
  (Link(): every soup program that links the main FS, `#define WIRE` + `#define POINTS`, each with its own DEFORM twin; prewarmed at model
  swap through a one-point dummy set) run the whole soup stage at that spot, so a point rides its triangle through every deform, letter
  move, shard / piece flight, clone and the morph pair, and draw GL_POINTS as round sprites anti-aliased over their last pixel: diameter
  = max(1.5, 2 x Wire Thickness) px (a line spans both sides of its edge), constant on screen; lifted 1.2 % of the fitted size toward the
  camera so the solid's own depth (Back Hidden) does not eat the sprite. The lines' look is theirs: Wire Color (+ Color Mix), Wireframe Mix
  = strength, Glow / Bloom (the same HDR wire buffer: `wireDraws`, pass 4), Fade, Back (Hidden = the solid's depth first; X-Ray = none).
  Vertices = one point per vertex welded by position (`WeldByPosition`, shared with the W5 edge flags) per region (the whole mesh, each
  piece); Feature Vertices = both ends of every W5 feature edge; Surface = N points by area over the whole mesh + N over the fractured
  copy (it stands in while pieces fly), fixed seed. Built per model + source on a worker (8 cached sets; `PrepareSlot` kUsePoints builds
  every filled slot's ahead of its hop); until a set lands no point draws (never a wait). Point Twinkle: per-point flicker, ~1 twinkle /
  s (each point's rate rounded to whole twinkles per 1000), clock `clk.pointTwinkle` wraps at 1000 (a w14_wraps row). Points skip the
  projector picture and the lava cracks (no surface), like Wire Only. Measured (tools/w6_points.py, harness 1080p, skull 31k tris):
  solid 0.77 GPU ms; Points Only Vertices 1.68, Surface 100k 2.35, 400k 4.97; + Glow 3 + Bloom 3 3.20 (100k).
- STAGE 2 THE TRANSITION PACK (Amrit: "how many transitions types between models can be added?"; no hard limit: one dropdown, no
  param room; "add, then prune"): Transition Style 5..22 appended (0..4 keep their numbers: saved clips keep their style). Style Order
  Cycle / Random go round all 23 (`kStyleCount`); Random keeps its per-hop hash. While Transition Play loops (Loop / Ping-Pong), every
  pass round the chain moves the hop index on by H (`chainLoops`, an integer kept modulo 23, reset by a restart, 0 with Play Off): a
  4-slot loop met only 4 styles before; now all 23 come round (Debug Log CHAIN lines). No new dial.
  - Every new style is slider-driven (scrubbing back retraces, holding still is still; the Glitch "clock" steps with the slider: 30
    steps per hop), per side (`TransitionPack` in Renderer.cpp: side 1 = the hop's outgoing model, 2 = the incoming one), per hop of
    the chain (the pair's per-slot data: the other slot's liquid SDF, shards + ranks, cloud). e = the hop eased by Trans Ease; each
    style's ramps are fixed stretches of e (`Rm`, smoothstepped `Sm`); the mid-hop swap is the W1 dissolve noise (`Swap`).
  - MORPH programs (`src/Morph.cpp`, `#define MORPH` in kSoupVS / kMainFS): progSoupM / MI + progShadowSoupM / MI, linked inside
    Init's twin window (so they have DEFORM, WIRE and POINTS twins); samplers 27 (the other model's liquid SDF, sampler3D), 28 (its
    shard data), 29 / 30 (shard ranks: this model's / the other's; RG32F = rank of shard i 0..1 bottom-up, the shard of rank i; built
    with the shards, `UpdateShards`). VS side `uMorphVS` 1 shrinkwrap, 2 pieces, 3 implode, 4 glitch, 5 ripple, 6 spin swap; FS side
    `uMorphMask` 1 scan, 2 glitch, 3 ripple, 4 burn (a front: the outgoing model shows ahead of it, the incoming one behind). Drawn only
    during such a hop; casters use progShadowSoupM (shadows morph too); the mirror floor draws the pair unmorphed.
  - The cloud (styles 5 Points, 17 Swarm): per slot 65,536 area-weighted points sorted by an order key (48 height bands bottom-up,
    round the up axis inside each), raw xyz + face normal + uv + part (worker, ~20 ms); colours baked once per slot from the part
    base colour maps (one point per texel, a draw per part; `UpdateCloud`); point i of the outgoing cloud flies to point i of the
    incoming one. Points: lit dots (Wire Color + Color Mix, size = Wire Thickness like the Stage 1 points, Point Twinkle) over the
    scene, + light (Wire Glow) added at alpha 0; Swarm: light only (Firefly Color / Size / Glow / Twinkle), every 4th point.
  - Until a style's per-slot data is on the GPU (cloud, ranks, the other SDF) its hop dissolves: never a wait (`MorphPairReady`).
    `ChainUses` builds them ahead: Points / Swarm kUseCloud, Pieces kUseShards, Smoke kUseSim, every slot's liquid SDF as before.
  - Classic bundles: Swarm, Burn, Lava read Wow-only dials (Firefly, Fire, Lava) and dissolve there (`StyleHere`). Smoke runs in every
    bundle (the sim is in every bundle).
  - SMOKE: the sim is a live simulation, so its smoke is not slider-driven: while the hop moves, the mesh dissolves (slider-driven)
    and the plugin emits Emit Inside + half as much Emit Rim at a rate set by how fast the hop moves (`SmokeStyle`: |de| / dt x 6, max
    10; holding still emits nothing); the sim runs (engine forced to Sim) and is lit (Smoke Ambient 1.2 when it is 0) through the hop
    and a 4 s tail (the light fades over its last 1.5 s).
  - WIRE TWIN DISCARD (see Hard-won rules): the dissolve, the cutout and the fronts cut through `wireCut` in the WIRE twins.
  - Measured (tools/w6_trans.py): 18 styles x 3 pairs (skull -> SciFi helmet and text -> skull in LoadEhr, skull -> Crown 1918 in Wow),
    9-step strips forward then backward: bit-identical at every step (Smoke: its mesh, Smoke Amount 0); t 0 / 1 vs the plain models
    max 0; onsets t 0.01 / 0.99 mean <= 0.024 (max <= 75: the DEFORM twin's normals at the first twist / melt). Harness 1080p GPU ms
    mid-hop (t 0.5, skull -> helmet, Wow; rest 0.82, Dissolve 1.22): Points 1.18, Shrinkwrap 0.78, Wire 2.66, Voxel 1.04, Low Poly
    1.10, Pieces 0.89, Implode 0.34, Twist 1.75, Melt 1.59, Scan 0.79, Glitch 0.89, Ripple 0.79, Swarm 0.89, Smoke 2.07, Burn 2.42,
    Lava 2.48, Spin Swap 0.84, Fold 3.06. Text and logos: a logo lives only in slot A (Model Source), so a text -> logo chain does not
    exist; text -> skull stands in. (W7: Shrinkwrap and Fold retired, Twist / Melt rebuilt, Lava on both models: see W7.)

## W7 (2026-10-08): the transition review (Amrit: "Twist and Melt has this glitch. Can the twist be more twisted and the melt be more melted and perhaps a sparkle that kicks in between the models shifting? Lava didn't work on the skull, so it look weird. Fold can go and also Shrink Wrap. Rest stays.")
- THE GLITCH (measured, `tools/w7_trans.py scrub|spikes|realtime`): NOT the plain -> DEFORM-twin switch at a hop's ends (the first
  1/400 of a hop changes 0.0000 mean, max 1). It was the MID-HOP SWAP: the dissolve noise (Fbm, the main FS) clusters near 0.5, so the
  pack's `Swap` (cut moved evenly over e 0.4 .. 0.6) swapped the whole surface in t 0.48 .. 0.53 (~0.3 s of a 6 s hop: outgoing 22.2k
  -> 0 px, incoming 0 -> 54k px), between a model twisted one way and one twisted the other: per-step change 13.4 vs the hop's own
  2.1. FIX: `Cut()` walks each model's own noise quantiles (`CpuGeometry::swapQ`, 129 of them over 16k area-weighted surface points,
  built by the load worker; 16 were too coarse at the skewed tails: 1/16 of the surface still popped in one step), so the share swapped
  follows the progress. Every `Swap` style uses it (+ the Points / Swarm mesh <-> cloud cut); Dissolve (0) is unchanged.
- TWIST (uMorphVS 7, `MorphTwist`): the model winds round the Deform Axis, the ends +-pi (1 + 1.5 Spread) at full wind (Spread 4:
  7 turns end to end), wrung thin across the axis (to 1 - min(0.8, 0.35 + 0.12 Spread)), the leading end first (Trans Stagger: > 0.5
  the top, < 0.5 the bottom, |Stagger - 0.5| x 2 = how far ahead); the height is normalised by the model's own extent along the axis
  (`CpuGeometry::fitBox`: a wide model was fitted short, so the far end never started). Wound by e 0.4; swap 0.32 .. 0.68; the incoming
  model arrives wound the SAME way and unwinds from e 0.6 (W6 twisted them opposite ways: the swap showed two mirror spirals).
- MELT (uMorphVS 8, `MorphMelt`): sags (gravity: u^2), the Drip columns run ahead (x (0.4 + 0.6 Spread)), every point lands in a puddle
  on the floor plane (Floor Height, `uMorphPlane` = the world plane in fitted space) by e 0.4: a lens of radius 1.1 + 0.6 Spread fitted
  units whose shape depends only on the direction round the centre and the radius share, so every model's puddle is the SAME shape and
  the swap (e 0.4 .. 0.6) is a colour cross-fade inside it; the incoming model rises out of it from e 0.6. Puddle normals face up.
- Both through the MORPH programs: normals by finite differences of the field (as DEFORM), the TANGENT = the file's tangent carried
  through the field (the tangent squared to the normal changed normal-mapped shading at a hop's end: max 75 / 255; the same fix in the
  DEFORM twin: a deform's first step on the SciFi helmet 821 -> 545 px > 2, the rest is real sub-pixel motion). Measured: swap window
  vs the hop's own motion 0.24 .. 0.97 (6 cases, `w7_trans.py spikes`); Arena's own C8 / C9 dials played (Ping-Pong 6 s, Hold 1.5 s,
  Spin Y 3, 60 Hz): max per-frame change W6 15.4 / 13.5 -> W7 2.72 / 2.60.
- LAVA (style 20): only the outgoing model ever cracked, so a skull in slot B (Arena's C16: Nefertiti -> Deer Skull, Ping-Pong) never
  cracked (Lava Glow 3 vs 0: 0 px on the incoming side) and arrived as white blotches (the dissolve edge glow: 22-32 % of its pixels
  near-white at t 0.55-0.6). Now both sides carry the lava: the outgoing model cracks (front by e 0.45), splits open (0.6 x Lava Split,
  e 0.25 .. 0.7) and dissolves away (0.5 .. 0.9); the incoming one shows through from 0.25 still split and glowing, closes (0.4 .. 0.85),
  heals and cools (0.6 .. 1). No dissolve edge glow in this style (the cracks are the edge). Classic bundles: still Dissolve.
- TRANS SPARKLE (Model group, every bundle, 0..10, default 0): sparkles on both models round the style's swap point (`SparklePeak`: Lava
  0.55, every other style 0.5, in eased progress e; up from peak - 0.2, gone by peak + 0.2). Each model's sparkles = a Points Only,
  glow-only draw of it (`sparkFs`, Surface 25k, a share 0.06 + 0.024 x Sparkle of each part's points, 2.4 .. 5.4 px), so the POINTS
  twins' soup stage puts them on the twisted / melted / split / flying surface; into the W5 HDR wire buffer as light at alpha 0 behind
  the pair's depth (Wire Bloom and the global Bloom catch them), colour = the surface lifted halfway to white, twinkling 3 / s (x 3 of the
  Point Twinkle clock: still whole twinkles per 1000). Light 1.2 x Sparkle x the envelope; Spin Swap / Implode dim each model's
  sparkles with what is left of it (else they pile into one white dot). With sparkles and no Wire Glow the wire buffer adds at Glow 1
  (edge case: Wire Glow 0 + Wire Bloom > 0 mid-sparkle lights the lines too). 0 = no draw, no pass.
- RETIRED: Shrinkwrap (6) and Fold (22) are TOMBSTONES (CONTRACT 7.2a, TOROFIELD-050): not in the list ("~" in ParamList.inc), values
  kept, every other style keeps its number; a clip holding 6 / 22 dissolves (`LiveStyle`). Cycle / Random go round `kLiveStyles` (21).
  Gone with them: MORPH vs 1, the 3D SDF sampler on unit 27 and its dummy, the Fold case (Deform's Mirror Fold stays).

## W28: Amrit's Wow review (`tools/w28_review.py`: strip / speed / lag (HEAD vs this build) / timeline / arena; FIXES = the fresh clips, lasers gone)
- TIME SCALES (1 = the W26 pace, 2 = crazy; Amrit: "Soooooft I want, Crazy as max MAYBE"): Fire Speed (default 0.5: sim time x it,
  the substep shortened to kSimDt x Fire Speed so a slow fire still steps every frame (skipping frames juddered); burst decay and
  the light's flicker too; Fire off = the sim byte-identical), Liquid Speed (0.25: the wobble), Physics Speed (0.35, Wow only;
  classic keeps 1: shard flight, Bounce playback, reform; the beat cycle's timing stays in show time). Defaults: Burn Rate 3 -> 2,
  Burst Amount 4 -> 2. Measured (`w28_review.py speed / lag`): liquid wobble motion x0.27-0.30, shard burst peak 9.7 -> 5.1 per
  frame and ~2.5 s -> ~6 s; fire lag motion x0.75 at 0.5-0.6 (the volume's per-frame jitter makes frame diffs useless for fire).
- THE CLOCK (the root of "WAY too fast"): Arena 7.28 sends FF_SET_TIME in MILLISECONDS (measured 2026-10-03: ~434,378,739 and
  +10..15 per 10..15 ms frame); the harness sends seconds. Since W1 the plugin read it as seconds, so EVERY Arena frame hit the 0.1 s
  dt clamp: all motion in Arena ran 6-12x the dials (and followed Arena's frame rate); harness renders were right, so no offline test
  could see it. Now ProcessOpenGL votes the unit from host step / wall step (~1000 = ms, ~1 = s, the harness's fast frames vote
  neither; `msVotes`, `hostScale`). Proof: harness `hostunit ms` (W28) + `realtime`: Spin Y 30 for 3 s lands at 90 deg with either
  unit (HEAD with ms: far past it), verify/w28/clock/ms_vs_s.png. Amrit chose the fix knowing every clip slows to its dials.
- TRANSITION PLAY (every bundle): Off / Once On Trigger / Ping-Pong / Loop + Transition Time (s, default 6); the slider is ignored
  while it plays; smoothstepped; the clock holds until Model B is on the GPU; `clk.transPlay` (Once holds at T, Ping-Pong wraps at
  2T, Loop at T + min(1.5, T / 4)). A TRIGGER = (WatchTrigger) the instance's first frame, the host clock going back, or no frame for
  > 0.3 s; each logs `TRIG ...` and restarts Transition Play + spreads the lava. Evidence (Arena 7.28 log, 2026-10-02 21:28-21:33):
  most clip triggers create a NEW instance (InitGL, new address), but four clips in a row reused one instance (model swaps, no
  InitGL). Param bursts (>= 40 sets between two frames) are logged, not acted on. Arena click test (2026-10-03, Amrit clicking,
  me logging): Arena makes the instance when a clip is created / copied (InitGL, ~999 param sets, one frame) and keeps it; the
  first click reached the plugin only as "not drawn for 1704 s" (restart: the morph played); a RE-CLICK OF A PLAYING CLIP reaches
  the plugin not at all (no new instance, no clock step, no param burst), and the selected clip keeps rendering for the clip-panel
  preview, so even eject + re-click may show no gap. So the re-click is done ARENA-SIDE: the clip's Transition gets a timeline
  animation (Play Once & Clear, PlayModeAway Restart, Duration = the morph's seconds, in 0 out 1; clip transport Play Once & Hold)
  and Transition Play is Off. Proven: click -> 0..1 over 6 s, re-click mid-play -> back to 0 and again, then holds 1 (REST poll).
  `clip_position` as the phase source is refused for FFGL source params. Set with the arena MCP (`ARENA_ANIM` in w28_review.py).
- LAVA HOLDS: W26's "comes and goes" = C14's Crack On Beat (Every 8) healing on every other beat (measured: bare 6-8 s), a fresh clip
  showing full cracks (Crack Spread 1 by hand) that a Crack Start press wiped and regrew, the skull's spin carrying the Lava Light's
  shafts round (with Spin Y 0 the glow is flat to +-2 % over 12 s). Now: a trigger spreads the cracks from nothing (the seed the
  first Crack Start would pick), Crack On Beat cracks once then FLARES on the beat (+80 %, 0.33 s), only Crack Reverse heals; flow
  drift 0.4 -> 0.12 / s, its temperature swing +-15 % -> +-10 %. Lava Pulse was not added (Wow FX has no free param).

## Performance (W29)
- INSTRUMENT: Debug Log on (per instance) = one `PERF` log line per second: fps = 1 / the median interval between this instance's
  ProcessOpenGL calls (what the host really delivers), slow end = 1 / the p95 interval, cpu ms inside ProcessOpenGL, and Render's
  passes' CPU ms (`src/Perf.*`: prep shadow keysh proj caus main floor sim vol comp light post). Off = no clock read, no GL call.
  `tools/w29_perf.py watch <tag>` (one 3 s system-wide Metal System Trace per playing instance, record only), `export <tag>` (slow,
  heavy CPU: after the bench), `report <tag> [since]` (per instance: PERF medians + Arena GPU busy from the trace, ATMOZ-018's
  method), `offline <tag> <layer:col> ...` (replays an Arena clip's CURRENT values in the harness; harness `setnext` = a picker's
  Sat/Bri/Alpha). LOADEHR_PERF_FINISH=1 (offline) = glFinish at every pass boundary: per-pass GPU, but the GPU clocks down
  between finishes (P1's main pass read 22, 5 and 1 ms in three runs): a ranking at best, never a cost.
- NO GPU CLOCK IN GL: Apple's GL returns 0 for every GL_TIMESTAMP (glQueryCounter and glGetInteger64v; M3 Max, "GL 4.1 Metal -
  90.5"); GL_TIME_ELAPSED cannot nest in a host's own query (the harness holds one) and under-reads (harness: 0.06 ms for the
  719k-tri mask frame). Arena's GPU time = the Metal System Trace (Instruments' own profiler; xctrace, all processes, nothing attached).
- BEFORE YOU READ A NUMBER (all measured W29, MBP M3 Max, 1920x1080 comp, 120 Hz ProMotion panel, Wire closed):
  * the SELECTED clip renders for the clip-panel preview even when ejected (full cost, PERF lines keep coming), and an MCP
    `clip open` / copy leaves the new clip selected (CANYONRUN-006): park the selection on an empty slot before any null arm;
  * another Claude session's offline harness on this machine shows in the trace as its own process (Fanous's held 9-67 % of the
    GPU during W29): check the trace's per-process split before trusting any number;
  * Arena's FPS & Stats read 120 for clips whose delivered rate was ~80-100 by BOTH the plugin's call interval and the trace's frame
    period (P1: 91.7 vs 90.6 fps): on this setup the readout is not the composition rate.
  * true null (nothing selected / playing): Arena ~0.2 ms GPU per frame. A still 719k-tri mask clip (MaskEhr): Arena ~5 ms GPU per frame.
- `glGetUniformLocation` = 0.08 us per call (standalone CGL probe, 150-uniform program): the ~150 lookups per DrawModel cost ~12 us.
  Not worth caching (measured W29).
- PER-FRAME PASSES (Renderer::Render order; "when" = what makes it run, else it returns at once). CPU = PERF medians in Arena:
  | pass | runs when | depends on | CPU ms in Arena |
  |---|---|---|---|
  | prep: input copy + mips | FX and the input is used (env / skin / glass / sim project / projector) | input texture | - |
  | prep: face readback | FX Face Track (PBO + fence, never waits) | input | - |
  | prep: LODs / shards / bounce / letters / dust / swarm | the model is ready; each checks its own dirty state | dials, model | 0.1-0.3 (dust 400k 0.3) |
  | shadow | a volume light (A/B/C) on with shadows: every frame, 1 or 6 (cube) model draws per light | lights, pose, shape | 0.2-0.6 |
  | keysh | Shadow Catcher > 0 or Smoke Key Light | key dir, pose | in shadow |
  | proj / caus | Proj Enable / Caustics > 0 | projector, glass parts | ~0.1 |
  | main | always: clear, (model swap: prewarm), the model, copies, ghosts, dust, liquid, MSAA resolve | everything | 0.2-0.7 |
  | floor | Mirror Floor / Silhouette / Trail Echoes | | - |
  | sim | depth copy when a volume / bolt / swarm / room is on; sim substeps (0-3 + prewarm) while Engine Sim | dt, warp | 0.3-1.3 |
  | vol | a volume light, smoke ambient / key, projector beam, trip look | | 0.1-0.2 |
  | comp | always (one full-screen pass; haze / caustic floor variant) | | 0.1-0.2 |
  | light | motes, mirror LEDs, fireflies / bolts | | 0.1-0.3 |
  | post | bloom / flares | | - |

## Reading notes (W20)
- **Crane, Llamas, Tariq, GPU Gems 3 ch.30 (3D fluids):** grid processed slice by slice as pixel shaders; MacCormack =
  two semi-Lagrangian steps (forward, backward) + half the error, clamped to the min / max of the source neighbourhood;
  Jacobi pressure with adjustable iterations; temperature buoyancy f = (k_T (T - T_amb) - k_d rho) up; obstacles:
  solid cells take the obstacle velocity, pressure at a solid neighbour = the cell's own (free slip); ray-march at
  half-voxel steps with jitter. "Math is cheap compared to bandwidth": higher-order advection on a low-res grid.
- **Fedkiw, Stam, Jensen 2001:** vorticity confinement f = eps h (N x w), w = curl u, N = grad|w| / |grad|w||; puts back
  the small swirls numerical dissipation removes.
- **Stam 1999 (Stable Fluids):** semi-Lagrangian advection is unconditionally stable (trace back, interpolate);
  projection = solve lap(p) = div u, u -= grad p.

## Text and logos (W21)
- Model Source (Model group, first row) = Model File / Text / Logo. Text / Logo build a SPEC string ("gen:" + key=value
  fields, `MakeSpec`) from every mesh-changing param; slot 0 loads the spec through the normal LoadJob -> ModelData ->
  GpuModel path (cache key = spec + mtime of the font / logo file), so every feature works on it. A new spec starts at once
  when slot 0 is idle; while a build runs, edits coalesce and the latest starts when it lands (no backlog). The Letters
  dials are not in the spec (they animate, never rebuild). The eye finder is skipped for specs.
- Text: CoreText. Font (dynamic list of installed families) writes Font Name (TEXT, the truth); Font Style lists the
  family's faces sorted light -> heavy, upright before italic (index 0 = Default). Font File (.ttf/.otf/.ttc) overrides.
  Letter Spacing = kCTTrackingAttributeName (a kCTKernAttributeName, even 0, switches the font's pair kerning OFF).
  ROWS: Arena 7.28 gives every FFGL TEXT param a ONE-LINE field (REST ParamString; Return commits; no switch exists, the
  W21 TextProbe proved a newline in the default changes nothing). So '|' starts a new row (Amrit's pick); real line
  breaks (REST / paste) also do. Arena sends the field on commit only (Return / leaving it), not per keystroke.
- Logo: SVG (nanosvg; fills + fill rule, gradients = first stop, strokes skipped, no <text>), PDF and PDF-compatible .ai
  (CGPDFScanner: m l c v y h re, f F f* B B* b b*, n S s, cm q Q, rg g k sc scn, form XObjects; page 1), PNG / JPG (traced:
  field = alpha, else darkness on light; <=512 px; components = groups; interpolated marching squares, Douglas-Peucker,
  Chaikin; the image maps onto every face, sides sample 3 px inside the edge). EPS: "EPS not supported on this macOS".
- Extrusion: Clipper2 boundary contours (the source fill rule; W2, was libtess2) -> collinear points dropped -> caps; bevel profiles Flat /
  Round / Chamfer / Inset as inset rings (miter limit 4); the bevel's width is capped per shape at 0.8 x area / perimeter
  (wider insets fold); side normals = length-weighted mean of 2 or 4 edges, sharp above 35 deg. Parts Front / Back / Side /
  Bevel (Part N controls). File Colours = a palette texture (4x4 px per shape). Sizes: 1 = the text line / logo height.
- Letters: every glyph / logo shape is a GROUP (`triGroup`, `CpuGeometry::groupBox`). Shatter = one shard per group.
  Type On / Wave / Spin / Hop = a per-letter transform TBO (3 RGBA32F texels: pivot + scale, quaternion, offset) read by
  the `#define LETTERS` soup programs (progSoupL / LI, progShadowSoupL / LI; units 19 / 20). The plugin forces shape.on
  while letters move; at rest the plain programs draw (regress max 0). Apple GL: re-attach a texture buffer
  (glTexBuffer) after every glBufferData on its buffer, or it keeps reading the old storage.
- Clocks: Letter Wave wraps at 1 cycle, Letter Spin at 360 (Separate: whole-number factors -2, -1, 1, 2), Type Speed's cycle
  (type all, hold 2 s) restarts at 1 by design. In tools/w14_wraps.py.

## orient.txt (W17): a per-file side note, not a library
- `orient.txt` next to a model file: `Rotate X|Y|Z <deg>` lines, the Transform dials' order (X, then Y, then Z), applied to
  the geometry BEFORE auto-fit, so fit, eye finder and Light C see the front at +Z. The file itself is never touched. Read by
  its exact path next to the chosen file (no listing). Scans and Z-up exports need it.

## Hard-won rules (Apple GL-over-Metal)
- HOST CLOCK UNITS (W28): Arena 7.28's FF_SET_TIME is in MILLISECONDS, the harness's in seconds; the plugin votes the unit per
  instance (host step vs wall step). Never take dt straight from the host time. Test with harness `hostunit ms` + `realtime`.
- ALPHA CONTRACT (W23): Resolume reads a plugin's alpha by its FFGL API version (FFGL issue #65, Resolume dev). With API 2.3
  it gives premultiplied inputs and reads outputs as premultiplied. With 2.2 and below it reads a SOURCE's output as STRAIGHT
  (rgb x alpha, then over); effects / mixers are premultiplied either way. LoadEhr writes premultiplied, with additive light
  (smoke, beams, glow cloud, motes, bloom, flares) at alpha 0. So at 2.1 everything in the air vanished in Arena and smoke showed
  only over the mask. Both bundles declare 2.3 (`PluginInfo(..., 2, 3, ...)`); never go below it. Judge looks over a
  background, never only over black (harness `composite arena`, the default; `composite straight` = the old reading).
- `glGenerateMipmap` is CPU-blocking (5–16 ms per 2k texture): mips are built on the loader thread.
  Define every level with `glTexImage2D(nullptr)` first, then `glTexSubImage2D`; per-level re-specification is ~4× slower.
  Upload RGBA or R8 only — RGB costs a driver conversion.
- Geometry shaders are emulated and ~2.5× slower than the texture-buffer soup path. Don't reintroduce them.
- A `discard` in the middle of the fragment shader can corrupt later values (wire alpha read back 0);
  keep the wireframe discard at the very end, and take derivatives (`fwidth`) before any discard. W6: it struck again in the
  WIRE twins: the dissolve's mid-shader discard made the dissolve edge glow read 1 everywhere (a white incoming model in every
  dissolve-based hop with a wire on, W5's Dissolve + wire included). In the WIRE twins the dissolve / the pack's fronts now set
  `wireCut` and the alpha test at the very end cuts (`#ifdef WIRE`; the plain programs keep their discard).
- Program variants compile lazily on first draw (60–250 ms). The renderer draws one scissored triangle
  through every variant at model swap. The colour write mask is part of the pipeline — don't prewarm with glColorMask.
- The SDK clamps float defaults to 0..1 in `SetParamInfo`; ranged params reset `defaultFloatVal` afterwards
  (Arena sends real values for ranged params).
- Arena 7.28 truncates parameter NAMES to 16 characters. W2 renamed the three offenders; every name and every
  dynamic display name is ≤16 now (CapName in LoadEhrPlugin.cpp). Renaming an existing param orphans its saved
  value in comps. Resolume serialises FFGL params by NAME, so inserting params at new indices is safe.
- Arena 7.28 verified: FF_TYPE_FILE shows as a file parameter; FFT buffer shows as an "Audio FFT" source choice
  (default Composition); SetParamVisibility events work (unused part slots hide at load); instances in the
  same process share the GPU model cache (copied clips swap in at 0 ms).
- Model textures are stored top-row-first (t=0 is the image top); host input textures are bottom-up.
  Input projections in the shader return GL-convention v.
- W3 chain (Apple GL 4.1): every declared sampler must have a loadable texture of ITS type on a unit of its own. A
  buffer/cube sampler left on unit 0 next to a sampler2D makes GL refuse the whole draw (the soup path drew nothing).
  Samplers are pinned at link time (`BindShadowSamplers`: shadow 2D 11/12/17/18, cube 13/14, shard buffers 15/16)
  and get 1-texel dummies when unused. W11: four volume lights (A, B, C left, C right); only A/B can be point lights.
- The GLSL compiler is sensitive: even a never-taken branch in the soup vertex stage changed W1's explode render
  (2 → 2,124 px). Feature variants that touch hot code are separate PROGRAMS (`#define INSTANCED`), not branches.
  `flat` is a reserved word. Every new program variant is prewarmed at model swap through a 1-pixel scissor
  (volume, post, instanced, blended-instanced, dust): first use otherwise costs 30-160 ms.
- CPU geometry (positions/uvs/indices) is kept after upload (`CpuGeometry`, shared via the GPU cache): shards, dust
  points and LODs are all built from it on worker threads.

## Parameter map (index order = Resolume display order; groups contiguous, Debug last)
Source of truth: `src/ParamList.inc` (one X-macro list builds the enum and the definitions; FX-only lines sit under
`#if LOADEHR_FX`). Resolume stores values by NAME: never rename a param; inserting at new indices is safe.
Generated by `tools/param_map.py` from the harness `params` dump of the four bundles (W26; each column = that bundle's FFGL
index, "—" = not carried). **LoadEhr 504 params, FX 508, Wow 498, Wow FX 512.**
Group order: Model, Text, Logo, Extrude, Letters, Transform, Camera, Camera Live, Motion, Material, Surface, Parts, Shape, Deform, Copies,
Lighting, Volume, Smoke Sim, Trip, Fire, Lightning, Swarm, Lava, Caustics, Light A, Light B, Light C, Spots, Projector, Post, Floor,
Silhouette, FX, Output, Input + Face (FX), Audio, Debug. (W26 also added Liquid to Model and Shatter Physics to Shape; W2 Shatter Mode
to Shape, the Deform group, the projector's video dials; the Mirror Room group went.)
Part slots: "Part N <field>" is renamed to "<material> <field>" (cut to 16 chars) at load; unused slots are hidden;
Skin is hidden in the source build. "Pose Data" is a hidden TEXT param that stores the four camera poses; "C Placed For"
(hidden TEXT) records the model Light C was placed for (the auto-place on C Enable runs once per model).
Colours: every "<X> Color" row starts a HUE / SAT / BRI / ALPHA quartet = ONE picker in Arena (REST shows it as one
`[color]` param under the HUE name; the SAT/BRI/ALPHA names are not shown).

| LoadEhr | FX | Wow | Wow FX | Group | Name | Type | Range | Default |
|---|---|---|---|---|---|---|---|---|
| 0 | 0 | 0 | 0 | Model | Model Source | OPT | Model File / Text / Logo | 0 |
| 1 | 1 | 1 | 1 | Model | Model A | FILE | fbx obj gltf glb | - |
| 2 | 2 | 2 | 2 | Model | Status A | TEXT | - | - |
| 3 | 3 | 3 | 3 | Model | Reload A | EVENT | - | - |
| 4 | 4 | 4 | 4 | Model | Model B | FILE | fbx obj gltf glb | - |
| 5 | 5 | 5 | 5 | Model | Status B | TEXT | - | - |
| 6 | 6 | 6 | 6 | Model | Reload B | EVENT | - | - |
| 7 | 7 | 7 | 7 | Model | Model C | FILE | fbx obj gltf glb | - |
| 8 | 8 | 8 | 8 | Model | Status C | TEXT | - | - |
| 9 | 9 | 9 | 9 | Model | Reload C | EVENT | - | - |
| 10 | 10 | 10 | 10 | Model | Model D | FILE | fbx obj gltf glb | - |
| 11 | 11 | 11 | 11 | Model | Status D | TEXT | - | - |
| 12 | 12 | 12 | 12 | Model | Reload D | EVENT | - | - |
| 13 | 13 | 13 | 13 | Model | Auto Fit | BOOL | - | 1 |
| 14 | 14 | 14 | 14 | Model | Fit To | OPT | All Parts / Visible Parts | 1 |
| 15 | 15 | 15 | 15 | Model | Normal Flip Y | BOOL | - | 0 |
| 16 | 16 | 16 | 16 | Model | Max Texture Size | OPT | Full / 4K / 2K | 0 |
| 17 | 17 | 17 | 17 | Model | Transition | STD | 0..1 | 0 |
| 18 | 18 | 18 | 18 | Model | Chain Loop | BOOL | - | 0 |
| 19 | 19 | 19 | 19 | Model | Transition Style | OPT | Dissolve / Shatter / Dust / Slice / Liquid / Points / Wire / Voxel / Low Poly / Pieces / Implode / Twist / Melt / Scan / Glitch / Ripple / Swarm / Smoke / Burn / Lava / Spin Swap | 0 |
| 20 | 20 | 20 | 20 | Model | Style Order | OPT | Fixed / Cycle / Random | 0 |
| 21 | 21 | 21 | 21 | Model | Transition Play | OPT | Off / Once On Trigger / Ping-Pong / Loop | 0 |
| 22 | 22 | 22 | 22 | Model | Transition Time | STD | 0.5..30 | 6 |
| 23 | 23 | 23 | 23 | Model | Hold Time | STD | 0..60 | 0 |
| 24 | 24 | 24 | 24 | Model | Trans Ease | STD | 0..1 | 1 |
| 25 | 25 | 25 | 25 | Model | Trans Stagger | STD | 0..1 | 0.85 |
| 26 | 26 | 26 | 26 | Model | Trans Spread | STD | 0..4 | 0.6 |
| 27 | 27 | 27 | 27 | Model | Trans Sparkle | STD | 0..10 | 0 |
| — | — | 28 | 28 | Model | Liquid Goo | STD | 0..1 | 0.6 |
| — | — | 29 | 29 | Model | Liquid Look | OPT | Chrome / Paint | 0 |
| — | — | 30 | 30 | Model | Liquid Color | HUE | 0..1 | 0.08 |
| — | — | 31 | 31 | Model | Liquid Sat | SAT | 0..1 | 0.6 |
| — | — | 32 | 32 | Model | Liquid Bri | BRI | 0..1 | 1 |
| — | — | 33 | 33 | Model | Liquid Tint Mix | ALPHA | 0..1 | 0 |
| — | — | 34 | 34 | Model | Liquid Wobble | STD | 0..1 | 0.4 |
| — | — | 35 | 35 | Model | Liquid Speed | STD | 0..2 | 0.25 |
| 28 | 28 | 36 | 36 | Text | Text | TEXT | - | - |
| 29 | 29 | 37 | 37 | Text | Font | OPT | Use Font Name / Academy Engraved LET / Al Bayan / Al Nile / Al Tarikh / American Typewriter / Andale Mono / Apple Braille / Apple Chancery / Apple Color Emoji / Apple SD Gothic Neo / Apple Symbols / AppleGothic / AppleMyungjo / Arial / Arial Black / Arial Hebrew / Arial Hebrew Scholar / Arial Narrow / Arial Rounded MT Bold / Arial Unicode MS / Avenir / Avenir Next / Avenir Next Condensed / Ayuthaya / Baghdad / Bangla MN / Bangla Sangam MN / Baskerville / Beirut / Big Caslon / Bodoni 72 / Bodoni 72 Oldstyle / Bodoni 72 Smallcaps / Bodoni Ornaments / Bradley Hand / Brush Script MT / Chalkboard / Chalkboard SE / Chalkduster / Charter / Cochin / Comic Sans MS / Copperplate / Corsiva Hebrew / Courier New / Damascus / DecoType Naskh / Devanagari MT / Devanagari Sangam MN / Didot / DIN Alternate / DIN Condensed / Diwan Kufi / Diwan Thuluth / Euphemia UCAS / Farah / Farisi / Futura / Galvji / GB18030 Bitmap / Geeza Pro / Geneva / Georgia / Gill Sans / Grantha Sangam MN / Gujarati MT / Gujarati Sangam MN / Gurmukhi MN / Gurmukhi MT / Gurmukhi Sangam MN / Heiti SC / Heiti TC / Helvetica / Helvetica Neue / Herculanum / Hiragino Maru Gothic ProN / Hiragino Mincho ProN / Hiragino Sans / Hiragino Sans GB / Hoefler Text / Impact / InaiMathi / ITF Devanagari / ITF Devanagari Marathi / JetBrains Mono / Kailasa / Kannada MN / Kannada Sangam MN / Kefa III / Khmer MN / Khmer Sangam MN / Kohinoor Bangla / Kohinoor Devanagari / Kohinoor Gujarati / Kohinoor Telugu / Kokonor / Krungthep / KufiStandardGK / Lao MN / Lao Sangam MN / Lucida Grande / Luminari / Malayalam MN / Malayalam Sangam MN / Marker Felt / Menlo / Microsoft Sans Serif / Mishafi / Mishafi Gold / Monaco / Mshtakan / Mukta Mahee / Muna / Myanmar MN / Myanmar Sangam MN / Nadeem / New Peninim MT / Noteworthy / Noto Nastaliq Urdu / Noto Sans Batak / Noto Sans Kannada / Noto Sans Myanmar / Noto Sans NKo / Noto Sans Oriya / Noto Sans Syriac / Noto Sans Tagalog / Noto Serif Myanmar / Optima / Oriya MN / Oriya Sangam MN / Palatino / Papyrus / Party LET / Phosphate / PingFang HK / PingFang MO / PingFang SC / PingFang TC / Plantagenet Cherokee / PT Mono / PT Sans / PT Sans Caption / PT Sans Narrow / PT Serif / PT Serif Caption / Raanana / Rockwell / Sana / Sathu / Savoye LET / Shree Devanagari 714 / SignPainter / Silom / Sinhala MN / Sinhala Sangam MN / Skia / Snell Roundhand / Songti SC / Songti TC / STIX Two Math / STIX Two Text / STSong / Studio Manager Big Font / Studio Manager ChFont / Studio Manager Dot Font 1 / Studio Manager Dot Font 3 / Sukhumvit Set / Symbol / Tahoma / Tamil MN / Tamil Sangam MN / Telugu MN / Telugu Sangam MN / Thonburi / Times New Roman / Trattatello / Trebuchet MS / Verdana / Waseem / Webdings / Wingdings / Wingdings 2 / Wingdings 3 / Yamaha EBM 10 / Yamaha EBM 8 / Zapf Dingbats / Zapfino | 0 |
| 30 | 30 | 38 | 38 | Text | Font Name | TEXT | - | - |
| 31 | 31 | 39 | 39 | Text | Font Style | OPT | Default / UltraLight / UltraLight Italic / Thin / Thin Italic / Light / Light Italic / Regular / Italic / Medium / Medium Italic / Bold / Condensed Bold / Bold Italic / Condensed Black | 0 |
| 32 | 32 | 40 | 40 | Text | Font File | FILE | fbx obj gltf glb | - |
| 33 | 33 | 41 | 41 | Text | Align | OPT | Left / Center / Right | 1 |
| 34 | 34 | 42 | 42 | Text | Line Spacing | STD | 0.5..3 | 1 |
| 35 | 35 | 43 | 43 | Text | Letter Spacing | STD | -0.3..2 | 0 |
| 36 | 36 | 44 | 44 | Text | Curve Detail | STD | 0..1 | 0.5 |
| 37 | 37 | — | — | Logo | Logo File | FILE | fbx obj gltf glb | - |
| 38 | 38 | — | — | Logo | Logo Colours | OPT | File Colours / Parts | 0 |
| 39 | 39 | — | — | Logo | Trace Threshold | STD | 0..1 | 0.5 |
| 40 | 40 | — | — | Logo | Trace Smooth | STD | 0..1 | 0.5 |
| 41 | 41 | — | — | Logo | Trace Invert | BOOL | - | 0 |
| 42 | 42 | — | — | Extrude | Extrude Depth | STD | 0.01..2 | 0.25 |
| 43 | 43 | — | — | Extrude | Bevel Size | STD | 0..0.2 | 0.02 |
| 44 | 44 | — | — | Extrude | Bevel Segments | STD | 1..8 | 3 |
| 45 | 45 | — | — | Extrude | Bevel Shape | OPT | Flat / Round / Chamfer / Inset | 1 |
| 46 | 46 | — | — | Extrude | Back Cap | BOOL | - | 1 |
| 47 | 47 | — | — | Extrude | Outline Only | BOOL | - | 0 |
| 48 | 48 | — | — | Extrude | Wall Thickness | STD | 0.005..0.3 | 0.04 |
| 49 | 49 | — | — | Letters | Type On | STD | 0..1 | 1 |
| 50 | 50 | — | — | Letters | Type Speed | STD | 0..30 | 0 |
| 51 | 51 | — | — | Letters | Letter Stagger | STD | 0..1 | 0.5 |
| 52 | 52 | — | — | Letters | Letter Wave | STD | 0..2 | 0 |
| 53 | 53 | — | — | Letters | Wave Speed | STD | 0..5 | 1 |
| 54 | 54 | — | — | Letters | Letter Spin | STD | -30..30 | 0 |
| 55 | 55 | — | — | Letters | Spin Mode | OPT | Together / Separate | 0 |
| 56 | 56 | — | — | Letters | Letter Hop | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| 57 | 57 | — | — | Letters | Hop Height | STD | 0..2 | 0.5 |
| 58 | 58 | 45 | 45 | Transform | Rotate X | STD | -180..180 | 0 |
| 59 | 59 | 46 | 46 | Transform | Rotate Y | STD | -180..180 | 0 |
| 60 | 60 | 47 | 47 | Transform | Rotate Z | STD | -180..180 | 0 |
| 61 | 61 | 48 | 48 | Transform | Spin X | STD | -30..30 | 0 |
| 62 | 62 | 49 | 49 | Transform | Spin Y | STD | -30..30 | 0 |
| 63 | 63 | 50 | 50 | Transform | Spin Z | STD | -30..30 | 0 |
| 64 | 64 | 51 | 51 | Transform | Reset Spin | EVENT | - | - |
| 65 | 65 | 52 | 52 | Transform | Pos X | STD | -3..3 | 0 |
| 66 | 66 | 53 | 53 | Transform | Pos Y | STD | -3..3 | 0 |
| 67 | 67 | 54 | 54 | Transform | Pos Z | STD | -10..10 | 0 |
| 68 | 68 | 55 | 55 | Transform | Scale | STD | 0..6 | 1 |
| 69 | 69 | 56 | 56 | Camera | FOV | STD | 5..150 | 35 |
| 70 | 70 | 57 | 57 | Camera | Distance | STD | 0.3..20 | 4 |
| 71 | 71 | 58 | 58 | Camera | Orbit Yaw | STD | -180..180 | 0 |
| 72 | 72 | 59 | 59 | Camera | Orbit Pitch | STD | -89..89 | 0 |
| 73 | 73 | — | — | Camera Live | Cut Every | OPT | Off / 1 Beat / 2 Beats / 4 Beats / 8 Beats | 0 |
| 74 | 74 | — | — | Camera Live | Cut Angles | STD | 2..8 | 6 |
| 75 | 75 | — | — | Camera Live | Angle Spread | STD | 0..180 | 50 |
| 76 | 76 | — | — | Camera Live | Cut Order | OPT | Random / Sequence | 0 |
| 77 | 77 | — | — | Camera Live | Beat Shake | STD | 0..4 | 0 |
| 78 | 78 | — | — | Camera Live | Shake Decay | STD | 0.02..2 | 0.25 |
| 79 | 79 | — | — | Camera Live | Vertigo | STD | -1..1 | 0 |
| 80 | 80 | — | — | Camera Live | Pose Glide | STD | 0..10 | 1.5 |
| 81 | 81 | — | — | Camera Live | Store Pose 1 | EVENT | - | - |
| 82 | 82 | — | — | Camera Live | Store Pose 2 | EVENT | - | - |
| 83 | 83 | — | — | Camera Live | Store Pose 3 | EVENT | - | - |
| 84 | 84 | — | — | Camera Live | Store Pose 4 | EVENT | - | - |
| 85 | 85 | — | — | Camera Live | Go Pose 1 | EVENT | - | - |
| 86 | 86 | — | — | Camera Live | Go Pose 2 | EVENT | - | - |
| 87 | 87 | — | — | Camera Live | Go Pose 3 | EVENT | - | - |
| 88 | 88 | — | — | Camera Live | Go Pose 4 | EVENT | - | - |
| 89 | 89 | — | — | Camera Live | Pose Data | TEXT (hidden) | - | - |
| 90 | 90 | 60 | 60 | Motion | Wobble Amount | STD | 0..2 | 0 |
| 91 | 91 | 61 | 61 | Motion | Wobble Speed | STD | 0..5 | 0.5 |
| 92 | 92 | 62 | 62 | Motion | Beat Bounce | STD | 0..2 | 0 |
| 93 | 93 | 63 | 63 | Motion | Beat Nod | STD | 0..2 | 0 |
| 94 | 94 | 64 | 64 | Motion | Audio Gain | STD | 0..10 | 1 |
| 95 | 95 | 65 | 65 | Motion | Audio > Scale | STD | 0..3 | 0 |
| 96 | 96 | 66 | 66 | Motion | Audio > Glow | STD | 0..10 | 0 |
| 97 | 97 | 67 | 67 | Motion | Audio > Explode | STD | 0..3 | 0 |
| 98 | 98 | 68 | 68 | Material | Tint Color | HUE | 0..1 | 0 |
| 99 | 99 | 69 | 69 | Material | Tint Sat | SAT | 0..1 | 0 |
| 100 | 100 | 70 | 70 | Material | Tint Bri | BRI | 0..1 | 1 |
| 101 | 101 | 71 | 71 | Material | Tint Mix | ALPHA | 0..1 | 0 |
| 102 | 102 | 72 | 72 | Material | Hue Shift | HUE | 0..1 | 0 |
| 103 | 103 | 73 | 73 | Material | Saturation | STD | 0..4 | 1 |
| 104 | 104 | 74 | 74 | Material | Brightness | STD | 0..6 | 1 |
| 105 | 105 | 75 | 75 | Material | Metal Override | BOOL | - | 0 |
| 106 | 106 | 76 | 76 | Material | Metallic Value | STD | 0..1 | 1 |
| 107 | 107 | 77 | 77 | Material | Roughness Scale | STD | 0..4 | 1 |
| 108 | 108 | 78 | 78 | Material | Normal Strength | STD | 0..6 | 1 |
| 109 | 109 | 79 | 79 | Material | Height Displace | STD | -0.5..0.5 | 0 |
| 110 | 110 | 80 | 80 | Material | Emissive Glow | STD | 0..10 | 1 |
| 111 | 111 | 81 | 81 | Material | Rim Glow | STD | 0..10 | 0 |
| 112 | 112 | 82 | 82 | Material | Rim Color | HUE | 0..1 | 0.104167 |
| 113 | 113 | 83 | 83 | Material | Rim Sat | SAT | 0..1 | 0.4 |
| 114 | 114 | 84 | 84 | Material | Rim Bri | BRI | 0..1 | 1 |
| 115 | 115 | 85 | 85 | Material | Rim Alpha | ALPHA | 0..1 | 1 |
| 116 | 116 | — | — | Surface | Oil Film | STD | 0..2 | 0 |
| 117 | 117 | — | — | Surface | Film Thickness | STD | 0.1..3 | 1 |
| 118 | 118 | — | — | Surface | Film Angle Shift | STD | 0..2 | 1 |
| 119 | 119 | — | — | Surface | Film Swirl | STD | 0..2 | 1 |
| 120 | 120 | — | — | Surface | Glitter | STD | 0..10 | 0 |
| 121 | 121 | — | — | Surface | Glitter Density | STD | 0..1 | 0.35 |
| 122 | 122 | — | — | Surface | Glitter Size | STD | 0.1..6 | 1 |
| 123 | 123 | — | — | Surface | Glitter Twinkle | STD | 0..1 | 0.2 |
| 124 | 124 | — | — | Surface | Glitter On Metal | STD | 0..1 | 0.8 |
| 125 | 125 | — | — | Surface | Liquid Chrome | STD | 0..1 | 0 |
| 126 | 126 | — | — | Surface | Chrome Ripple | STD | 0..3 | 0.6 |
| 127 | 127 | 86 | 86 | Surface | Glass | STD | 0..1 | 0 |
| 128 | 128 | 87 | 87 | Surface | Glass IOR | STD | 1..2.5 | 1.45 |
| 129 | 129 | 88 | 88 | Surface | Glass Dispersion | STD | 0..2 | 0.4 |
| 130 | 130 | 89 | 89 | Surface | Glass Color | HUE | 0..1 | 0.5625 |
| 131 | 131 | 90 | 90 | Surface | Glass Sat | SAT | 0..1 | 0.08 |
| 132 | 132 | 91 | 91 | Surface | Glass Bri | BRI | 0..1 | 1 |
| 133 | 133 | 92 | 92 | Surface | Glass Alpha | ALPHA | 0..1 | 1 |
| 134 | 134 | — | — | Surface | Hologram | STD | 0..1 | 0 |
| 135 | 135 | — | — | Surface | Holo Scanlines | STD | 0..4 | 1 |
| 136 | 136 | — | — | Surface | Holo Flicker | STD | 0..2 | 0.5 |
| 137 | 137 | — | — | Surface | Holo Split | STD | 0..4 | 1 |
| 138 | 138 | — | — | Surface | Holo Glitch | STD | 0..4 | 0 |
| 139 | 139 | — | — | Surface | Holo Color | HUE | 0..1 | 0.52846 |
| 140 | 140 | — | — | Surface | Holo Sat | SAT | 0..1 | 1 |
| 141 | 141 | — | — | Surface | Holo Bri | BRI | 0..1 | 0.762759 |
| 142 | 142 | — | — | Surface | Holo Alpha | ALPHA | 0..1 | 1 |
| 143 | 143 | — | — | Surface | Heat Glow | STD | 0..10 | 0 |
| 144 | 144 | — | — | Surface | Heat Temp | STD | 600..12000 | 1900 |
| 145 | 145 | — | — | Surface | Heat Spread | STD | 0..1 | 0.7 |
| 146 | 146 | — | — | Surface | Pattern | OPT | Off / Stripes / Cells / Noise | 0 |
| 147 | 147 | — | — | Surface | Pattern Amount | STD | 0..10 | 2 |
| 148 | 148 | — | — | Surface | Pattern Scale | STD | 0.2..40 | 5 |
| 149 | 149 | — | — | Surface | Pattern Speed | STD | -4..4 | 0.5 |
| 150 | 150 | — | — | Surface | Pattern Color | HUE | 0..1 | 0.520833 |
| 151 | 151 | — | — | Surface | Pattern Sat | SAT | 0..1 | 0.8 |
| 152 | 152 | — | — | Surface | Pattern Bri | BRI | 0..1 | 1 |
| 153 | 153 | — | — | Surface | Pattern Alpha | ALPHA | 0..1 | 1 |
| 154 | 154 | — | — | Surface | Pattern Beat | STD | 0..4 | 0 |
| 155 | 155 | — | — | Surface | Pattern Audio | STD | 0..4 | 0 |
| 156 | 156 | 93 | 93 | Parts | Part 1 Visible | BOOL | - | 1 |
| 157 | 157 | 94 | 94 | Parts | Part 1 Hue Shift | STD | 0..1 | 0 |
| 158 | 158 | 95 | 95 | Parts | Part 1 Glow | STD | 0..5 | 0 |
| 159 | 159 | 96 | 96 | Parts | Part 1 Skin | BOOL | - | 0 |
| 160 | 160 | 97 | 97 | Parts | Part 1 Surface | STD | 0..1 | 1 |
| 161 | 161 | 98 | 98 | Parts | Part 2 Visible | BOOL | - | 1 |
| 162 | 162 | 99 | 99 | Parts | Part 2 Hue Shift | STD | 0..1 | 0 |
| 163 | 163 | 100 | 100 | Parts | Part 2 Glow | STD | 0..5 | 0 |
| 164 | 164 | 101 | 101 | Parts | Part 2 Skin | BOOL | - | 0 |
| 165 | 165 | 102 | 102 | Parts | Part 2 Surface | STD | 0..1 | 1 |
| 166 | 166 | 103 | 103 | Parts | Part 3 Visible | BOOL | - | 1 |
| 167 | 167 | 104 | 104 | Parts | Part 3 Hue Shift | STD | 0..1 | 0 |
| 168 | 168 | 105 | 105 | Parts | Part 3 Glow | STD | 0..5 | 0 |
| 169 | 169 | 106 | 106 | Parts | Part 3 Skin | BOOL | - | 0 |
| 170 | 170 | 107 | 107 | Parts | Part 3 Surface | STD | 0..1 | 1 |
| 171 | 171 | 108 | 108 | Parts | Part 4 Visible | BOOL | - | 1 |
| 172 | 172 | 109 | 109 | Parts | Part 4 Hue Shift | STD | 0..1 | 0 |
| 173 | 173 | 110 | 110 | Parts | Part 4 Glow | STD | 0..5 | 0 |
| 174 | 174 | 111 | 111 | Parts | Part 4 Skin | BOOL | - | 0 |
| 175 | 175 | 112 | 112 | Parts | Part 4 Surface | STD | 0..1 | 1 |
| 176 | 176 | 113 | 113 | Parts | Part 5 Visible | BOOL | - | 1 |
| 177 | 177 | 114 | 114 | Parts | Part 5 Hue Shift | STD | 0..1 | 0 |
| 178 | 178 | 115 | 115 | Parts | Part 5 Glow | STD | 0..5 | 0 |
| 179 | 179 | 116 | 116 | Parts | Part 5 Skin | BOOL | - | 0 |
| 180 | 180 | 117 | 117 | Parts | Part 5 Surface | STD | 0..1 | 1 |
| 181 | 181 | 118 | 118 | Parts | Part 6 Visible | BOOL | - | 1 |
| 182 | 182 | 119 | 119 | Parts | Part 6 Hue Shift | STD | 0..1 | 0 |
| 183 | 183 | 120 | 120 | Parts | Part 6 Glow | STD | 0..5 | 0 |
| 184 | 184 | 121 | 121 | Parts | Part 6 Skin | BOOL | - | 0 |
| 185 | 185 | 122 | 122 | Parts | Part 6 Surface | STD | 0..1 | 1 |
| 186 | 186 | 123 | 123 | Parts | Part 7 Visible | BOOL | - | 1 |
| 187 | 187 | 124 | 124 | Parts | Part 7 Hue Shift | STD | 0..1 | 0 |
| 188 | 188 | 125 | 125 | Parts | Part 7 Glow | STD | 0..5 | 0 |
| 189 | 189 | 126 | 126 | Parts | Part 7 Skin | BOOL | - | 0 |
| 190 | 190 | 127 | 127 | Parts | Part 7 Surface | STD | 0..1 | 1 |
| 191 | 191 | 128 | 128 | Parts | Part 8 Visible | BOOL | - | 1 |
| 192 | 192 | 129 | 129 | Parts | Part 8 Hue Shift | STD | 0..1 | 0 |
| 193 | 193 | 130 | 130 | Parts | Part 8 Glow | STD | 0..5 | 0 |
| 194 | 194 | 131 | 131 | Parts | Part 8 Skin | BOOL | - | 0 |
| 195 | 195 | 132 | 132 | Parts | Part 8 Surface | STD | 0..1 | 1 |
| 196 | 196 | 133 | 133 | Shape | Shatter | EVENT | - | - |
| 197 | 197 | 134 | 134 | Shape | Reform | EVENT | - | - |
| 198 | 198 | 135 | 135 | Shape | Shatter On Beat | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| 199 | 199 | 136 | 136 | Shape | Shard Count | STD | 8..512 | 64 |
| 200 | 200 | 137 | 137 | Shape | Shatter Mode | OPT | Letters / Pieces / Pieces + Letters | 1 |
| 201 | 201 | 138 | 138 | Shape | Shatter Force | STD | 0..10 | 1.5 |
| 202 | 202 | 139 | 139 | Shape | Shatter Gravity | STD | 0..20 | 2 |
| 203 | 203 | 140 | 140 | Shape | Shard Spin | STD | 0..20 | 3 |
| 204 | 204 | 141 | 141 | Shape | Reform Time | STD | 0.05..10 | 1.2 |
| 205 | 205 | 142 | 142 | Shape | Shatter Physics | OPT | Analytic / Bounce | 0 |
| 206 | 206 | 143 | 143 | Shape | Bounciness | STD | 0..1 | 0.5 |
| 207 | 207 | 144 | 144 | Shape | Floor Friction | STD | 0..1 | 0.3 |
| 208 | 208 | — | — | Shape | Slice Count | STD | 2..64 | 12 |
| 209 | 209 | — | — | Shape | Slice Gap | STD | 0..2 | 0 |
| 210 | 210 | — | — | Shape | Slice Axis | OPT | X / Y / Z | 1 |
| 211 | 211 | — | — | Shape | Slice Wave | STD | 0..4 | 0 |
| 212 | 212 | — | — | Shape | Voxel | STD | 0..1 | 0 |
| 213 | 213 | — | — | Shape | Voxel Size | STD | 0.005..0.5 | 0.06 |
| 214 | 214 | — | — | Shape | Low Poly | STD | 0..1 | 0 |
| 215 | 215 | — | — | Shape | Low Poly Size | STD | 0.01..0.8 | 0.15 |
| 216 | 216 | — | — | Shape | LowPoly Rot X | STD | -180..180 | 0 |
| 217 | 217 | — | — | Shape | LowPoly Rot Y | STD | -180..180 | 0 |
| 218 | 218 | — | — | Shape | LowPoly Rot Z | STD | -180..180 | 0 |
| 219 | 219 | — | — | Shape | LowPoly Spin X | STD | -30..30 | 0 |
| 220 | 220 | — | — | Shape | LowPoly Spin Y | STD | -30..30 | 0 |
| 221 | 221 | — | — | Shape | LowPoly Spin Z | STD | -30..30 | 0 |
| 222 | 222 | — | — | Shape | Facet Tumble | OPT | Off / Together / Separate | 0 |
| 223 | 223 | — | — | Shape | Tumble X | STD | -30..30 | 0 |
| 224 | 224 | — | — | Shape | Tumble Y | STD | -30..30 | 0 |
| 225 | 225 | — | — | Shape | Tumble Z | STD | -30..30 | 0 |
| 226 | 226 | — | — | Shape | Tumble Random | STD | 0..1 | 0.5 |
| 227 | 227 | — | — | Shape | Tumble Sway | STD | 0..1 | 0 |
| 228 | 228 | — | — | Shape | Tumble On Beat | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| 229 | 229 | — | — | Shape | Tumble Beat Flip | OPT | 90 / 180 | 0 |
| 230 | 230 | — | — | Shape | Facet Gap | STD | 0..1 | 0 |
| 231 | 231 | — | — | Shape | Tumble Reset | EVENT | - | - |
| 232 | 232 | — | — | Shape | Blow To Dust | EVENT | - | - |
| 233 | 233 | — | — | Shape | Dust Reform | EVENT | - | - |
| 234 | 234 | — | — | Shape | Dust On Beat | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| 235 | 235 | — | — | Shape | Dust Hold | STD | 0..30 | 0 |
| 236 | 236 | — | — | Shape | Dust | STD | 0..1 | 0 |
| 237 | 237 | — | — | Shape | Dust Count | OPT | 25k / 50k / 100k / 200k / 400k | 2 |
| 238 | 238 | — | — | Shape | Dust Wind | STD | 0..6 | 1 |
| 239 | 239 | — | — | Shape | Dust Size | STD | 0.3..8 | 1.5 |
| 240 | 240 | — | — | Shape | Contours | STD | 0..10 | 0 |
| 241 | 241 | — | — | Shape | Contour Mode | OPT | X / Y / Z / Height Map / Radial | 1 |
| 242 | 242 | — | — | Shape | Contour Spacing | STD | 0.005..0.5 | 0.06 |
| 243 | 243 | — | — | Shape | Contour Width | STD | 0.01..1 | 0.15 |
| 244 | 244 | — | — | Shape | Contour Speed | STD | -4..4 | 0.3 |
| 245 | 245 | — | — | Shape | Contour Color | HUE | 0..1 | 0.0784314 |
| 246 | 246 | — | — | Shape | Contour Sat | SAT | 0..1 | 0.85 |
| 247 | 247 | — | — | Shape | Contour Bri | BRI | 0..1 | 1 |
| 248 | 248 | — | — | Shape | Contour Alpha | ALPHA | 0..1 | 1 |
| — | — | 145 | 145 | Shape | Physics Speed | STD | 0..2 | 0.35 |
| 249 | 249 | 146 | 146 | Deform | Deform Amount | STD | 0..2 | 1 |
| 250 | 250 | 147 | 147 | Deform | Deform Axis | OPT | X / Y / Z | 1 |
| 251 | 251 | 148 | 148 | Deform | Taper | STD | -2..2 | 0 |
| 252 | 252 | 149 | 149 | Deform | Twist | STD | -8..8 | 0 |
| 253 | 253 | 150 | 150 | Deform | Bend | STD | -4..4 | 0 |
| 254 | 254 | 151 | 151 | Deform | Squash | STD | -1..1 | 0 |
| 255 | 255 | 152 | 152 | Deform | Squash Beat | STD | 0..2 | 0 |
| 256 | 256 | 153 | 153 | Deform | Shear | STD | -2..2 | 0 |
| 257 | 257 | 154 | 154 | Deform | Bulge | STD | -1..2 | 0 |
| 258 | 258 | 155 | 155 | Deform | Spherize | STD | 0..1 | 0 |
| 259 | 259 | 156 | 156 | Deform | Noise Warp | STD | 0..2 | 0 |
| 260 | 260 | 157 | 157 | Deform | Warp Scale | STD | 0.2..10 | 2 |
| 261 | 261 | 158 | 158 | Deform | Warp Speed | STD | 0..4 | 0.3 |
| 262 | 262 | 159 | 159 | Deform | Ripple | STD | 0..0.5 | 0 |
| 263 | 263 | 160 | 160 | Deform | Ripple Freq | STD | 1..80 | 14 |
| 264 | 264 | 161 | 161 | Deform | Ripple Speed | STD | -20..20 | 4 |
| 265 | 265 | 162 | 162 | Deform | Ripple Beat | STD | 0..4 | 0 |
| 266 | 266 | 163 | 163 | Deform | Melt | STD | 0..1 | 0 |
| 267 | 267 | 164 | 164 | Deform | Drip | STD | 0..3 | 1 |
| 268 | 268 | 165 | 165 | Deform | Glitch Shift | STD | 0..1 | 0 |
| 269 | 269 | 166 | 166 | Deform | Glitch Bands | STD | 2..64 | 12 |
| 270 | 270 | 167 | 167 | Deform | Glitch Rate | STD | 0..30 | 6 |
| 271 | 271 | 168 | 168 | Deform | Mirror Fold | OPT | Off / X / X+Y / X+Y+Z | 0 |
| 272 | 272 | 169 | 169 | Deform | Jelly | STD | 0..2 | 0 |
| 273 | 273 | 170 | 170 | Deform | Audio > Deform | STD | 0..4 | 0 |
| 274 | 274 | 171 | 171 | Wireframe | Wire Mode | OPT | Off / Over Surface / Wire Only / Wire + Dark Fill / Points Only / Points + Wire | 0 |
| 275 | 275 | 172 | 172 | Wireframe | Wireframe Mix | STD | 0..1 | 1 |
| 276 | 276 | 173 | 173 | Wireframe | Wire Thickness | STD | 0.2..8 | 1 |
| 277 | 277 | 174 | 174 | Wireframe | Wire Back | OPT | Hidden / X-Ray | 0 |
| 278 | 278 | 175 | 175 | Wireframe | Wire Edges | OPT | All Triangles / Feature Edges | 0 |
| 279 | 279 | 176 | 176 | Wireframe | Wire Color | HUE | 0..1 | 0.5 |
| 280 | 280 | 177 | 177 | Wireframe | Wire Sat | SAT | 0..1 | 1 |
| 281 | 281 | 178 | 178 | Wireframe | Wire Bri | BRI | 0..1 | 1 |
| 282 | 282 | 179 | 179 | Wireframe | Wire Color Mix | ALPHA | 0..1 | 0 |
| 283 | 283 | 180 | 180 | Wireframe | Wire Glow | STD | 0..10 | 0 |
| 284 | 284 | 181 | 181 | Wireframe | Wire Bloom | STD | 0..10 | 0 |
| 285 | 285 | 182 | 182 | Wireframe | Wire Bloom Size | STD | 0..1 | 0.5 |
| 286 | 286 | 183 | 183 | Wireframe | Wire Fade | STD | 0..1 | 0 |
| 287 | 287 | 184 | 184 | Wireframe | Point Source | OPT | Vertices / Feature Vertices / Surface 25k / Surface 100k / Surface 400k | 3 |
| 288 | 288 | 185 | 185 | Wireframe | Point Twinkle | STD | 0..1 | 0 |
| 289 | 289 | — | — | Copies | Clone Layout | OPT | Off / Ring / Grid / Tunnel / Carousel | 0 |
| 290 | 290 | — | — | Copies | Clone Count | STD | 1..64 | 8 |
| 291 | 291 | — | — | Copies | Clone Spacing | STD | 0.2..12 | 2.4 |
| 292 | 292 | — | — | Copies | Clone Spin Step | STD | -180..180 | 0 |
| 293 | 293 | — | — | Copies | Clone Hue Spread | STD | 0..1 | 0 |
| 294 | 294 | — | — | Copies | Clone Orbit | STD | -30..30 | 0 |
| 295 | 295 | — | — | Copies | Clone Scale | STD | 0.05..3 | 0.6 |
| 296 | 296 | — | — | Copies | Tunnel Fade | STD | 0..1 | 0.25 |
| 297 | 297 | — | — | Copies | Keep Original | BOOL | - | 1 |
| 298 | 298 | — | — | Copies | LOD Bias | STD | 0.1..4 | 1 |
| 299 | 299 | — | — | Copies | Trail Echoes | STD | 0..7 | 0 |
| 300 | 300 | — | — | Copies | Trail Delay | STD | 1..30 | 4 |
| 301 | 301 | — | — | Copies | Trail Decay | STD | 0..1 | 0.7 |
| 302 | 302 | — | — | Copies | Trail Offset X | STD | -0.1..0.1 | 0 |
| 303 | 303 | — | — | Copies | Trail Offset Y | STD | -0.1..0.1 | 0 |
| 304 | 304 | — | — | Copies | Ghosts | STD | 0..8 | 0 |
| 305 | 305 | — | — | Copies | Ghost Spacing | STD | 1..30 | 5 |
| 306 | 306 | — | — | Copies | Ghost Fade | STD | 0..1 | 0.6 |
| 307 | 307 | — | — | Copies | Trail Smear | STD | 0..1 | 0 |
| 308 | 308 | — | — | Copies | Trail Blur | STD | 0..2 | 0 |
| 309 | 309 | — | — | Copies | Trail Dust | STD | 0..1 | 0 |
| 310 | 310 | — | — | Copies | Trail Grain | STD | 1..12 | 2.5 |
| 311 | 311 | — | — | Copies | Trail Drift | STD | 0..2 | 0.15 |
| 312 | 312 | — | — | Copies | Trail Drift Dir | STD | -180..180 | 90 |
| 313 | 313 | — | — | Copies | Trail Fade Curve | OPT | Linear / Soft / Long Tail | 0 |
| 314 | 314 | — | — | Copies | Trail Glow | STD | 0..10 | 0 |
| 315 | 315 | — | — | Copies | Trail Color | HUE | 0..1 | 0.55 |
| 316 | 316 | — | — | Copies | Trail Sat | SAT | 0..1 | 0.6 |
| 317 | 317 | — | — | Copies | Trail Bri | BRI | 0..1 | 1 |
| 318 | 318 | — | — | Copies | Trail Alpha | ALPHA | 0..1 | 1 |
| 319 | 319 | — | — | Copies | Trail Color Mix | STD | 0..1 | 0 |
| 320 | 320 | 186 | 186 | Lighting | Key Azimuth | STD | -180..180 | 35 |
| 321 | 321 | 187 | 187 | Lighting | Key Elevation | STD | -90..90 | 35 |
| 322 | 322 | 188 | 188 | Lighting | Key Intensity | STD | 0..20 | 3 |
| 323 | 323 | 189 | 189 | Lighting | Key Color | HUE | 0..1 | 0.104167 |
| 324 | 324 | 190 | 190 | Lighting | Key Sat | SAT | 0..1 | 0.08 |
| 325 | 325 | 191 | 191 | Lighting | Key Bri | BRI | 0..1 | 1 |
| 326 | 326 | 192 | 192 | Lighting | Key Alpha | ALPHA | 0..1 | 1 |
| 327 | 327 | 193 | 193 | Lighting | Ambient | STD | 0..1 | 0.03 |
| 328 | 328 | 194 | 194 | Lighting | Env Intensity | STD | 0..10 | 1 |
| 329 | 329 | 195 | 195 | Lighting | Env Source | OPT | Studio / Input (FX) / Off | 0 |
| 330 | 330 | 196 | 196 | Lighting | Env Rotate | STD | -180..180 | 0 |
| 331 | 331 | 197 | 197 | Lighting | Exposure | STD | -5..5 | 0 |
| 332 | 332 | 198 | 198 | Lighting | Tonemap | OPT | ACES / Filmic / None | 0 |
| 333 | 333 | 199 | 199 | Volume | Vol Method | OPT | Raymarch / Screen Rays | 0 |
| 334 | 334 | 200 | 200 | Volume | Haze | STD | 0..2 | 0.12 |
| 335 | 335 | 201 | 201 | Volume | Smoke Amount | STD | 0..4 | 1 |
| 336 | 336 | 202 | 202 | Volume | Smoke Scale | STD | 0.1..10 | 1.5 |
| 337 | 337 | 203 | 203 | Volume | Smoke Speed | STD | 0..4 | 0.3 |
| 338 | 338 | 204 | 204 | Volume | Smoke Turbulence | STD | 0..4 | 1 |
| 339 | 339 | 205 | 205 | Volume | Smoke Rise | STD | -2..2 | 0.15 |
| 340 | 340 | 206 | 206 | Volume | Glow Cloud | STD | 0..10 | 1 |
| 341 | 341 | 207 | 207 | Volume | Smoke Ambient | STD | 0..4 | 0 |
| 342 | 342 | 208 | 208 | Volume | Smoke Key Light | STD | 0..4 | 0 |
| 343 | 343 | 209 | 209 | Volume | Volume Size | STD | 0.5..12 | 3.5 |
| 344 | 344 | 210 | 210 | Volume | Scatter G | STD | -0.9..0.99 | 0.6 |
| 345 | 345 | 211 | 211 | Volume | Shaft Length | STD | 0.2..20 | 6 |
| 346 | 346 | 212 | 212 | Volume | Motes | STD | 0..4 | 0 |
| 347 | 347 | 213 | 213 | Volume | Vol Quality | OPT | 16 / 32 / 64 / 128 | 1 |
| 348 | 348 | 214 | 214 | Volume | Vol Res | OPT | Full / Half / Quarter | 1 |
| 349 | 349 | 215 | 215 | Volume | Fog Blend | OPT | Add / Over | 0 |
| 350 | 350 | 216 | 216 | Volume | Drift Return | STD | 0..3 | 0.4 |
| 351 | 351 | 217 | 217 | Smoke Sim | Smoke Engine | OPT | Noise / Sim / Sim + Detail | 0 |
| 352 | 352 | 218 | 218 | Smoke Sim | Sim Res | OPT | 48 / 64 / 96 / 128 | 1 |
| 353 | 353 | 219 | 219 | Smoke Sim | Sim Box | STD | 1..6 | 2.5 |
| 354 | 354 | 220 | 220 | Smoke Sim | Sim Iterations | STD | 10..40 | 20 |
| 355 | 355 | 221 | 221 | Smoke Sim | Buoyancy | STD | 0..10 | 2 |
| 356 | 356 | 222 | 222 | Smoke Sim | Heat Decay | STD | 0..5 | 0.5 |
| 357 | 357 | 223 | 223 | Smoke Sim | Vorticity | STD | 0..10 | 2 |
| 358 | 358 | 224 | 224 | Smoke Sim | Smoke Fade | STD | 0..3 | 0.6 |
| 359 | 359 | 225 | 225 | Smoke Sim | Wind X | STD | -3..3 | 0 |
| 360 | 360 | 226 | 226 | Smoke Sim | Wind Y | STD | -3..3 | 0 |
| 361 | 361 | 227 | 227 | Smoke Sim | Wind Z | STD | -3..3 | 0 |
| 362 | 362 | 228 | 228 | Smoke Sim | Sim Turbulence | STD | 0..2 | 0.5 |
| 363 | 363 | 229 | 229 | Smoke Sim | Emit Eyes | STD | 0..10 | 0 |
| 364 | 364 | 230 | 230 | Smoke Sim | Eyes Heat | STD | 0..5 | 1 |
| 365 | 365 | 231 | 231 | Smoke Sim | Emit Behind | STD | 0..10 | 0 |
| 366 | 366 | 232 | 232 | Smoke Sim | Behind Heat | STD | 0..5 | 1 |
| 367 | 367 | 233 | 233 | Smoke Sim | Emit Rim | STD | 0..10 | 0 |
| 368 | 368 | 234 | 234 | Smoke Sim | Rim Heat | STD | 0..5 | 1 |
| 369 | 369 | 235 | 235 | Smoke Sim | Emit Inside | STD | 0..10 | 0 |
| 370 | 370 | 236 | 236 | Smoke Sim | Inside Heat | STD | 0..5 | 1 |
| 371 | 371 | 237 | 237 | Smoke Sim | Emit Offset | STD | 0..0.5 | 0 |
| 372 | 372 | 238 | 238 | Smoke Sim | Beat Puff | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| 373 | 373 | 239 | 239 | Smoke Sim | Puff Amount | STD | 0..10 | 3 |
| 374 | 374 | 240 | 240 | Smoke Sim | Audio > Emit | STD | 0..4 | 0 |
| 375 | 375 | 241 | 241 | Smoke Sim | Sim Reset | EVENT | - | - |
| 376 | 376 | 242 | 242 | Smoke Sim | Smoke Color | HUE | 0..1 | 0 |
| 377 | 377 | 243 | 243 | Smoke Sim | Smoke Sat | SAT | 0..1 | 0 |
| 378 | 378 | 244 | 244 | Smoke Sim | Smoke Bri | BRI | 0..1 | 1 |
| 379 | 379 | 245 | 245 | Smoke Sim | Smoke Alpha | ALPHA | 0..1 | 1 |
| 380 | 380 | 246 | 246 | Smoke Sim | Smoke Confine | STD | 0..1 | 0 |
| — | 381 | — | — | Smoke Sim | Project Input | STD | 0..1 | 0 |
| — | — | 247 | 247 | Trip | Trip Master | STD | 0..2 | 1 |
| — | — | 248 | 248 | Trip | Colour By | OPT | Off / Density / Heat / Speed / Swirl / Age / Height | 0 |
| — | — | 249 | 249 | Trip | Trip Palette | OPT | Rainbow / Acid / Neon / Fire / Ice / Picker Pair | 0 |
| — | — | 250 | 250 | Trip | Palette Shift | STD | -2..2 | 0 |
| — | — | 251 | 251 | Trip | Palette Spread | STD | 0..4 | 1 |
| — | — | 252 | 252 | Trip | Trip A Color | HUE | 0..1 | 0.83 |
| — | — | 253 | 253 | Trip | Trip A Sat | SAT | 0..1 | 1 |
| — | — | 254 | 254 | Trip | Trip A Bri | BRI | 0..1 | 1 |
| — | — | 255 | 255 | Trip | Trip A Alpha | ALPHA | 0..1 | 1 |
| — | — | 256 | 256 | Trip | Trip B Color | HUE | 0..1 | 0.45 |
| — | — | 257 | 257 | Trip | Trip B Sat | SAT | 0..1 | 1 |
| — | — | 258 | 258 | Trip | Trip B Bri | BRI | 0..1 | 1 |
| — | — | 259 | 259 | Trip | Trip B Alpha | ALPHA | 0..1 | 1 |
| — | — | 260 | 260 | Trip | Thin Film | STD | 0..2 | 0 |
| — | — | 261 | 261 | Trip | Thin Film Thick | STD | 0.1..3 | 1 |
| — | — | 262 | 262 | Trip | Prism | STD | 0..2 | 0 |
| — | — | 263 | 263 | Trip | Prism Angle | STD | -180..180 | 0 |
| — | — | 264 | 264 | Trip | Kaleido Smoke | STD | 1..16 | 1 |
| — | — | 265 | 265 | Trip | Kaleido Twist | STD | -360..360 | 0 |
| — | — | 266 | 266 | Trip | Kaleido Spin | STD | -30..30 | 0 |
| — | — | 267 | 267 | Trip | Fractal Fog | STD | 0..2 | 0 |
| — | — | 268 | 268 | Trip | Fractal Scale | STD | 0.2..4 | 1 |
| — | — | 269 | 269 | Trip | Fractal Iter | STD | 1..6 | 3 |
| — | — | 270 | 270 | Trip | Fractal Morph | STD | -30..30 | 6 |
| — | — | 271 | 271 | Trip | Neon Veins | STD | 0..10 | 0 |
| — | — | 272 | 272 | Trip | Vein Width | STD | 0..1 | 0.4 |
| — | — | 273 | 273 | Trip | Vein Color | HUE | 0..1 | 0.5 |
| — | — | 274 | 274 | Trip | Vein Sat | SAT | 0..1 | 1 |
| — | — | 275 | 275 | Trip | Vein Bri | BRI | 0..1 | 1 |
| — | — | 276 | 276 | Trip | Vein Alpha | ALPHA | 0..1 | 1 |
| — | — | 277 | 277 | Trip | Smoke Contours | STD | 0..10 | 0 |
| — | — | 278 | 278 | Trip | Contour Bands | STD | 2..32 | 8 |
| — | — | 279 | 279 | Trip | Band Sharpness | STD | 0..1 | 0.6 |
| — | — | 280 | 280 | Trip | Band Scroll | STD | -4..4 | 0.3 |
| — | — | 281 | 281 | Trip | Contour Style | OPT | Lines / Bands | 0 |
| — | — | 282 | 282 | Trip | Line Width | STD | 0.5..6 | 1.5 |
| — | — | 283 | 283 | Trip | Line Glow | STD | 0..4 | 1 |
| — | — | 284 | 284 | Trip | Blacklight | STD | 0..10 | 0 |
| — | — | 285 | 285 | Trip | Blacklight By | OPT | Density / Heat | 0 |
| — | — | 286 | 286 | Trip | Blacklight Color | HUE | 0..1 | 0.76 |
| — | — | 287 | 287 | Trip | Blacklight Sat | SAT | 0..1 | 0.85 |
| — | — | 288 | 288 | Trip | Blacklight Bri | BRI | 0..1 | 1 |
| — | — | 289 | 289 | Trip | Blacklight Alpha | ALPHA | 0..1 | 1 |
| — | — | 290 | 290 | Trip | Fire Ring | EVENT | - | - |
| — | — | 291 | 291 | Trip | Ring On Beat | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| — | — | 292 | 292 | Trip | Ring From | OPT | Eyes / Mouth / Behind | 0 |
| — | — | 293 | 293 | Trip | Ring Size | STD | 0.05..1 | 0.3 |
| — | — | 294 | 294 | Trip | Ring Speed | STD | 0..6 | 2 |
| — | — | 295 | 295 | Trip | Ring Spin | STD | -10..10 | 0 |
| — | — | 296 | 296 | Trip | Ring Smoke | STD | 0..10 | 4 |
| — | — | 297 | 297 | Trip | Smoke Ghosts | STD | 0..4 | 0 |
| — | — | 298 | 298 | Trip | Ghost Offset | STD | 0..3 | 1 |
| — | — | 299 | 299 | Trip | Ghost Scale | STD | 0.3..3 | 1.3 |
| — | — | 300 | 300 | Trip | Ghost Fade Time | STD | 0.1..5 | 1.5 |
| — | — | 301 | 301 | Trip | Whirlpool | STD | -5..5 | 0 |
| — | — | 302 | 302 | Trip | Hyperspace | STD | -5..5 | 0 |
| — | — | 303 | 303 | Trip | Hue Echo | STD | 0..1 | 0 |
| — | — | 304 | 304 | Trip | Echo Hue Step | STD | -0.1..0.1 | 0.02 |
| — | — | 305 | 305 | Trip | Time Warp | STD | 0..2 | 1 |
| — | — | 306 | 306 | Trip | Freeze | EVENT | - | - |
| — | — | 307 | 307 | Fire | Fire | STD | 0..1 | 0 |
| — | — | 308 | 308 | Fire | Fire Speed | STD | 0..2 | 0.5 |
| — | — | 309 | 309 | Fire | Burn Rate | STD | 0.2..10 | 2 |
| — | — | 310 | 310 | Fire | Flame Temp | STD | 1000..6000 | 2000 |
| — | — | 311 | 311 | Fire | Flame Height | STD | 0..4 | 1 |
| — | — | 312 | 312 | Fire | Flame Glow | STD | 0..10 | 2 |
| — | — | 313 | 313 | Fire | Fire Smoke | STD | 0..2 | 0.4 |
| — | — | 314 | 314 | Fire | Flame Tint Color | HUE | 0..1 | 0.6 |
| — | — | 315 | 315 | Fire | Flame Tint Sat | SAT | 0..1 | 0.8 |
| — | — | 316 | 316 | Fire | Flame Tint Bri | BRI | 0..1 | 1 |
| — | — | 317 | 317 | Fire | Flame Tint Mix | ALPHA | 0..1 | 0 |
| — | — | 318 | 318 | Fire | Fire Burst | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| — | — | 319 | 319 | Fire | Burst Amount | STD | 0..10 | 2 |
| — | — | 320 | 320 | Fire | Fire Light | STD | 0..10 | 2 |
| — | — | 321 | 321 | Lightning | Lightning Strike | EVENT | - | - |
| — | — | 322 | 322 | Lightning | Strike On Beat | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| — | — | 323 | 323 | Lightning | Strike Rate | STD | 0..10 | 0 |
| — | — | 324 | 324 | Lightning | Bolt Path | OPT | Crawl / Eye To Eye / To Floor / To Clones | 0 |
| — | — | 325 | 325 | Lightning | Bolt Count | STD | 1..8 | 2 |
| — | — | 326 | 326 | Lightning | Branching | STD | 0..1 | 0.5 |
| — | — | 327 | 327 | Lightning | Jaggedness | STD | 0..1 | 0.5 |
| — | — | 328 | 328 | Lightning | Bolt Life | STD | 0.05..2 | 0.4 |
| — | — | 329 | 329 | Lightning | Bolt Width | STD | 0.5..6 | 1.5 |
| — | — | 330 | 330 | Lightning | Bolt Glow | STD | 0..10 | 3 |
| — | — | 331 | 331 | Lightning | Bolt Color | HUE | 0..1 | 0.64 |
| — | — | 332 | 332 | Lightning | Bolt Sat | SAT | 0..1 | 0.55 |
| — | — | 333 | 333 | Lightning | Bolt Bri | BRI | 0..1 | 1 |
| — | — | 334 | 334 | Lightning | Bolt Alpha | ALPHA | 0..1 | 1 |
| — | — | 335 | 335 | Lightning | Bolt Flash | STD | 0..10 | 2 |
| — | — | 336 | 336 | Swarm | Swarm Count | OPT | Off / 2k / 5k / 10k / 20k / 40k | 0 |
| — | — | 337 | 337 | Swarm | Swarm Release | EVENT | - | - |
| — | — | 338 | 338 | Swarm | Release On Beat | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| — | — | 339 | 339 | Swarm | Release Time | STD | 0.2..10 | 2 |
| — | — | 340 | 340 | Swarm | Attraction | STD | 0..10 | 3 |
| — | — | 341 | 341 | Swarm | Flock | STD | 0..2 | 1 |
| — | — | 342 | 342 | Swarm | Swarm Speed | STD | 0..4 | 1 |
| — | — | 343 | 343 | Swarm | Swarm Offset | STD | 0..1 | 0.03 |
| — | — | 344 | 344 | Swarm | Firefly Size | STD | 0.5..8 | 2 |
| — | — | 345 | 345 | Swarm | Firefly Glow | STD | 0..10 | 3 |
| — | — | 346 | 346 | Swarm | Twinkle | STD | 0..1 | 0.5 |
| — | — | 347 | 347 | Swarm | Firefly Color | HUE | 0..1 | 0.14 |
| — | — | 348 | 348 | Swarm | Firefly Sat | SAT | 0..1 | 0.75 |
| — | — | 349 | 349 | Swarm | Firefly Bri | BRI | 0..1 | 1 |
| — | — | 350 | 350 | Swarm | Firefly Alpha | ALPHA | 0..1 | 1 |
| — | — | 351 | 351 | Swarm | Mask Visible | BOOL | - | 1 |
| — | — | 352 | 352 | Lava | Lava Cracks | STD | 0..1 | 0 |
| — | — | 353 | 353 | Lava | Crack Spread | STD | 0..1 | 1 |
| — | — | 354 | 354 | Lava | Crack Start | EVENT | - | - |
| — | — | 355 | 355 | Lava | Crack Reverse | EVENT | - | - |
| — | — | 356 | 356 | Lava | Crack On Beat | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| — | — | 357 | 357 | Lava | Crack Time | STD | 0.2..10 | 2 |
| — | — | 358 | 358 | Lava | Crack Scale | STD | 0.5..8 | 2.5 |
| — | — | 359 | 359 | Lava | Crack Width | STD | 0..1 | 0.3 |
| — | — | 360 | 360 | Lava | Lava Temp | STD | 800..4000 | 1600 |
| — | — | 361 | 361 | Lava | Lava Glow | STD | 0..10 | 3 |
| — | — | 362 | 362 | Lava | Lava Tint Color | HUE | 0..1 | 0.55 |
| — | — | 363 | 363 | Lava | Lava Tint Sat | SAT | 0..1 | 0.8 |
| — | — | 364 | 364 | Lava | Lava Tint Bri | BRI | 0..1 | 1 |
| — | — | 365 | 365 | Lava | Lava Tint Mix | ALPHA | 0..1 | 0 |
| — | — | 366 | 366 | Lava | Heat Haze | STD | 0..2 | 0.5 |
| — | — | 367 | 367 | Lava | Lava Split | STD | 0..1 | 0 |
| — | — | 368 | 368 | Lava | Lava Light | STD | 0..40 | 10 |
| — | — | 369 | 369 | Caustics | Caustics | STD | 0..4 | 0 |
| — | — | 370 | 370 | Caustics | Caustic Source | OPT | Glass Parts / Whole Mask | 0 |
| — | — | 371 | 371 | Caustics | Caustic IOR | STD | 1..2.5 | 1.5 |
| — | — | 372 | 372 | Caustics | Caustic Rainbow | STD | 0..2 | 0.5 |
| — | — | 373 | 373 | Caustics | Caustic Sharp | STD | 0..1 | 0.6 |
| — | — | 374 | 374 | Caustics | Caustic Ripple | STD | 0..1 | 0.3 |
| — | — | 375 | 375 | Caustics | Caustic Floor | STD | 0..2 | 1 |
| — | — | 376 | 376 | Caustics | Caustic Smoke | STD | 0..2 | 0.5 |
| — | — | 377 | 377 | Caustics | Caustic Surface | STD | 0..2 | 1 |
| — | — | 378 | 378 | Caustics | Caustic Color | HUE | 0..1 | 0.12 |
| — | — | 379 | 379 | Caustics | Caustic Sat | SAT | 0..1 | 0.15 |
| — | — | 380 | 380 | Caustics | Caustic Bri | BRI | 0..1 | 1 |
| — | — | 381 | 381 | Caustics | Caustic Alpha | ALPHA | 0..1 | 1 |
| 381 | 382 | 382 | 382 | Light A | A Enable | BOOL | - | 0 |
| 382 | 383 | 383 | 383 | Light A | A Type | OPT | Spot / Point | 0 |
| 383 | 384 | 384 | 384 | Light A | A Anchor To Mask | BOOL | - | 1 |
| 384 | 385 | 385 | 385 | Light A | A Pos X | STD | -6..6 | 0 |
| 385 | 386 | 386 | 386 | Light A | A Pos Y | STD | -6..6 | 0 |
| 386 | 387 | 387 | 387 | Light A | A Pos Z | STD | -6..6 | -1.5 |
| 387 | 388 | 388 | 388 | Light A | A Aim Mode | OPT | At Camera / At Mask / Free | 0 |
| 388 | 389 | 389 | 389 | Light A | A Aim Yaw | STD | -180..180 | 0 |
| 389 | 390 | 390 | 390 | Light A | A Aim Pitch | STD | -90..90 | 0 |
| 390 | 391 | 391 | 391 | Light A | A Cone Angle | STD | 1..170 | 45 |
| 391 | 392 | 392 | 392 | Light A | A Cone Soft | STD | 0..1 | 0.5 |
| 392 | 393 | 393 | 393 | Light A | A Intensity | STD | 0..100 | 12 |
| 393 | 394 | 394 | 394 | Light A | A Color | HUE | 0..1 | 0.105263 |
| 394 | 395 | 395 | 395 | Light A | A Sat | SAT | 0..1 | 0.38 |
| 395 | 396 | 396 | 396 | Light A | A Bri | BRI | 0..1 | 1 |
| 396 | 397 | 397 | 397 | Light A | A Alpha | ALPHA | 0..1 | 1 |
| 397 | 398 | 398 | 398 | Light A | A Hue Drift | STD | -0.7..0.7 | 0 |
| 398 | 399 | 399 | 399 | Light A | A Beat Pulse | STD | 0..4 | 0 |
| 399 | 400 | 400 | 400 | Light A | A Audio > Light | STD | 0..4 | 0 |
| 400 | 401 | 401 | 401 | Light B | B Enable | BOOL | - | 0 |
| 401 | 402 | 402 | 402 | Light B | B Type | OPT | Spot / Point | 0 |
| 402 | 403 | 403 | 403 | Light B | B Anchor To Mask | BOOL | - | 0 |
| 403 | 404 | 404 | 404 | Light B | B Pos X | STD | -6..6 | 2.5 |
| 404 | 405 | 405 | 405 | Light B | B Pos Y | STD | -6..6 | 0.6 |
| 405 | 406 | 406 | 406 | Light B | B Pos Z | STD | -6..6 | 1 |
| 406 | 407 | 407 | 407 | Light B | B Aim Mode | OPT | At Camera / At Mask / Free | 1 |
| 407 | 408 | 408 | 408 | Light B | B Aim Yaw | STD | -180..180 | 0 |
| 408 | 409 | 409 | 409 | Light B | B Aim Pitch | STD | -90..90 | 0 |
| 409 | 410 | 410 | 410 | Light B | B Cone Angle | STD | 1..170 | 45 |
| 410 | 411 | 411 | 411 | Light B | B Cone Soft | STD | 0..1 | 0.5 |
| 411 | 412 | 412 | 412 | Light B | B Intensity | STD | 0..100 | 12 |
| 412 | 413 | 413 | 413 | Light B | B Color | HUE | 0..1 | 0.592593 |
| 413 | 414 | 414 | 414 | Light B | B Sat | SAT | 0..1 | 0.45 |
| 414 | 415 | 415 | 415 | Light B | B Bri | BRI | 0..1 | 1 |
| 415 | 416 | 416 | 416 | Light B | B Alpha | ALPHA | 0..1 | 1 |
| 416 | 417 | 417 | 417 | Light B | B Hue Drift | STD | -0.7..0.7 | 0 |
| 417 | 418 | 418 | 418 | Light B | B Beat Pulse | STD | 0..4 | 0 |
| 418 | 419 | 419 | 419 | Light B | B Audio > Light | STD | 0..4 | 0 |
| 419 | 420 | 420 | 420 | Light C | C Enable | BOOL | - | 0 |
| 420 | 421 | 421 | 421 | Light C | C Place At Eyes | EVENT | - | - |
| 421 | 422 | 422 | 422 | Light C | C Fit Cone | EVENT | - | - |
| 422 | 423 | 423 | 423 | Light C | C Front Only | BOOL | - | 1 |
| 423 | 424 | 424 | 424 | Light C | C Behind Depth | STD | 0.02..2 | 0.3 |
| 424 | 425 | 425 | 425 | Light C | C Anchor To Mask | BOOL | - | 1 |
| 425 | 426 | 426 | 426 | Light C | C Mirror X | BOOL | - | 1 |
| 426 | 427 | 427 | 427 | Light C | C Left Pos X | STD | -6..6 | -0.3 |
| 427 | 428 | 428 | 428 | Light C | C Left Pos Y | STD | -6..6 | 0.2 |
| 428 | 429 | 429 | 429 | Light C | C Left Pos Z | STD | -6..6 | 0.2 |
| 429 | 430 | 430 | 430 | Light C | C Right Pos X | STD | -6..6 | 0.3 |
| 430 | 431 | 431 | 431 | Light C | C Right Pos Y | STD | -6..6 | 0.2 |
| 431 | 432 | 432 | 432 | Light C | C Right Pos Z | STD | -6..6 | 0.2 |
| 432 | 433 | 433 | 433 | Light C | C Aim Mode | OPT | Through Hole / At Camera / Free | 0 |
| 433 | 434 | 434 | 434 | Light C | C Aim Yaw | STD | -180..180 | 0 |
| 434 | 435 | 435 | 435 | Light C | C Aim Pitch | STD | -90..90 | 0 |
| 435 | 436 | 436 | 436 | Light C | C Cone Angle | STD | 1..170 | 20 |
| 436 | 437 | 437 | 437 | Light C | C Cone Soft | STD | 0..1 | 0.5 |
| 437 | 438 | 438 | 438 | Light C | C Intensity | STD | 0..100 | 6 |
| 438 | 439 | 439 | 439 | Light C | C Left Color | HUE | 0..1 | 0.105263 |
| 439 | 440 | 440 | 440 | Light C | C Left Sat | SAT | 0..1 | 0.38 |
| 440 | 441 | 441 | 441 | Light C | C Left Bri | BRI | 0..1 | 1 |
| 441 | 442 | 442 | 442 | Light C | C Left Alpha | ALPHA | 0..1 | 1 |
| 442 | 443 | 443 | 443 | Light C | C Link Colors | BOOL | - | 1 |
| 443 | 444 | 444 | 444 | Light C | C Right Color | HUE | 0..1 | 0.105263 |
| 444 | 445 | 445 | 445 | Light C | C Right Sat | SAT | 0..1 | 0.38 |
| 445 | 446 | 446 | 446 | Light C | C Right Bri | BRI | 0..1 | 1 |
| 446 | 447 | 447 | 447 | Light C | C Right Alpha | ALPHA | 0..1 | 1 |
| 447 | 448 | 448 | 448 | Light C | C Hue Drift | STD | -0.7..0.7 | 0 |
| 448 | 449 | 449 | 449 | Light C | C Beat Pulse | STD | 0..4 | 0 |
| 449 | 450 | 450 | 450 | Light C | C Audio > Light | STD | 0..4 | 0 |
| 450 | 451 | 451 | 451 | Light C | C Placed For | TEXT (hidden) | - | - |
| 451 | 452 | — | — | Spots | Spot Count | OPT | Off / 1 / 2 / 3 / 4 | 0 |
| 452 | 453 | — | — | Spots | Spot Intensity | STD | 0..100 | 25 |
| 453 | 454 | — | — | Spots | Spot Cone | STD | 2..90 | 14 |
| 454 | 455 | — | — | Spots | Spot Speed | STD | 0..15 | 3.75 |
| 455 | 456 | — | — | Spots | Spot Pattern | OPT | Pan / Circle / Figure 8 / Cross / Random | 0 |
| 456 | 457 | — | — | Spots | Spot Width | STD | 0..4 | 1.2 |
| 457 | 458 | — | — | Spots | Spot Rig Height | STD | -4..6 | 2.5 |
| 458 | 459 | — | — | Spots | Spot Rig Dist | STD | 0.5..10 | 3 |
| 459 | 460 | — | — | Spots | Spot Color | HUE | 0..1 | 0 |
| 460 | 461 | — | — | Spots | Spot Sat | SAT | 0..1 | 0 |
| 461 | 462 | — | — | Spots | Spot Bri | BRI | 0..1 | 1 |
| 462 | 463 | — | — | Spots | Spot Alpha | ALPHA | 0..1 | 1 |
| 463 | 464 | — | — | Spots | Spot Color Mode | OPT | Single / Rainbow / Pairs | 0 |
| 464 | 465 | — | — | Spots | Spot Beat Strobe | STD | 0..1 | 0 |
| 465 | 466 | — | — | Spots | Spot Beams | STD | 0..4 | 1 |
| — | — | 452 | 452 | Projector | Proj Enable | BOOL | - | 0 |
| — | — | — | 453 | Projector | Proj Source | OPT | Input / File | 0 |
| — | — | 453 | 454 | Projector | Proj File | FILE | fbx obj gltf glb | - |
| — | — | 454 | 455 | Projector | Video Speed | STD | -4..4 | 1 |
| — | — | 455 | 456 | Projector | Video Loop | OPT | Loop / Ping-Pong / Once / Scrub | 0 |
| — | — | 456 | 457 | Projector | Video Position | STD | 0..1 | 0 |
| — | — | 457 | 458 | Projector | Video Time | TEXT | - | - |
| — | — | 458 | 459 | Projector | Video Restart | EVENT | - | - |
| — | — | 459 | 460 | Projector | Video Alpha | OPT | Premultiplied / Straight | 0 |
| — | — | 460 | 461 | Projector | Proj Anchor | BOOL | - | 0 |
| — | — | 461 | 462 | Projector | Proj Pos X | STD | -6..6 | 0.8 |
| — | — | 462 | 463 | Projector | Proj Pos Y | STD | -6..6 | 1.2 |
| — | — | 463 | 464 | Projector | Proj Pos Z | STD | -6..6 | 4 |
| — | — | 464 | 465 | Projector | Proj Aim Mode | OPT | At Mask / Free | 0 |
| — | — | 465 | 466 | Projector | Proj Aim Yaw | STD | -180..180 | 0 |
| — | — | 466 | 467 | Projector | Proj Aim Pitch | STD | -90..90 | 0 |
| — | — | 467 | 468 | Projector | Proj Throw | STD | 5..120 | 30 |
| — | — | 468 | 469 | Projector | Proj Focus | STD | 0..1 | 0 |
| — | — | 469 | 470 | Projector | Proj Intensity | STD | 0..100 | 25 |
| — | — | 470 | 471 | Projector | Proj Color | HUE | 0..1 | 0 |
| — | — | 471 | 472 | Projector | Proj Sat | SAT | 0..1 | 0 |
| — | — | 472 | 473 | Projector | Proj Bri | BRI | 0..1 | 1 |
| — | — | 473 | 474 | Projector | Proj Alpha | ALPHA | 0..1 | 1 |
| — | — | 474 | 475 | Projector | Proj Beam | STD | 0..4 | 1 |
| — | — | 475 | 476 | Projector | Proj Surface | STD | 0..4 | 1 |
| 466 | 467 | 476 | 477 | Post | Bloom | STD | 0..10 | 0 |
| 467 | 468 | 477 | 478 | Post | Bloom Threshold | STD | 0..1 | 0.65 |
| 468 | 469 | 478 | 479 | Post | Bloom Radius | STD | 0..1 | 0.6 |
| 469 | 470 | 479 | 480 | Post | Flare Ghosts | STD | 0..10 | 0 |
| 470 | 471 | 480 | 481 | Post | Flare Streak | STD | 0..10 | 0 |
| 471 | 472 | 481 | 482 | Floor | Shadow Catcher | STD | 0..1 | 0 |
| 472 | 473 | 482 | 483 | Floor | Catcher Plane | OPT | Floor / Wall | 0 |
| 473 | 474 | 483 | 484 | Floor | Floor Height | STD | -4..2 | -1.02 |
| 474 | 475 | 484 | 485 | Floor | Wall Distance | STD | 0..8 | 1.5 |
| 475 | 476 | 485 | 486 | Floor | Shadow Soft | STD | 0..1 | 0.4 |
| 476 | 477 | 486 | 487 | Floor | Mirror Floor | STD | 0..1 | 0 |
| 477 | 478 | 487 | 488 | Floor | Mirror Blur | STD | 0..1 | 0.2 |
| 478 | 479 | 488 | 489 | Floor | Mirror Fade | STD | 0..1 | 0.2 |
| 479 | 480 | 489 | 490 | Floor | Floor Light | STD | 0..1 | 0.5 |
| 480 | 481 | 490 | 491 | Floor | Floor Camera | STD | 0..1 | 1 |
| 481 | 482 | — | — | Silhouette | Silhouette | STD | 0..1 | 0 |
| 482 | 483 | — | — | Silhouette | Sil Fill | STD | 0..1 | 0 |
| 483 | 484 | — | — | Silhouette | Sil Glow | STD | 0..10 | 2 |
| 484 | 485 | — | — | Silhouette | Sil Width | STD | 0.5..60 | 8 |
| 485 | 486 | — | — | Silhouette | Sil Color | HUE | 0..1 | 0.523809 |
| 486 | 487 | — | — | Silhouette | Sil Sat | SAT | 0..1 | 0.7 |
| 487 | 488 | — | — | Silhouette | Sil Bri | BRI | 0..1 | 1 |
| 488 | 489 | — | — | Silhouette | Sil Alpha | ALPHA | 0..1 | 1 |
| 489 | 490 | — | — | FX | Explode | STD | 0..3 | 0 |
| 490 | 491 | — | — | FX | Explode Random | STD | 0..1 | 0.5 |
| 491 | 492 | — | — | FX | Dissolve | STD | 0..1 | 0 |
| 492 | 493 | — | — | FX | Dissolve Glow | STD | 0..10 | 1 |
| 493 | 494 | — | — | FX | Toon Steps | STD | 0..8 | 0 |
| 494 | 495 | — | — | FX | Outline | STD | 0..1 | 0 |
| 495 | 496 | — | — | FX | Depth Fog | STD | 0..3 | 0 |
| 496 | 497 | 491 | 492 | Output | Background Color | HUE | 0..1 | 0 |
| 497 | 498 | 492 | 493 | Output | Background Sat | SAT | 0..1 | 0 |
| 498 | 499 | 493 | 494 | Output | Background Bri | BRI | 0..1 | 0 |
| 499 | 500 | 494 | 495 | Output | Background Alpha | ALPHA | 0..1 | 0 |
| 500 | 501 | 495 | 496 | Output | MSAA | OPT | 1x / 4x / 8x | 1 |
| — | 502 | — | 497 | Input | Input Use | OPT | Off / Background / Skin on flagged parts / Skin all | 0 |
| — | 503 | — | 498 | Input | Input Projection | OPT | Front planar / Model UV / Spherical | 0 |
| — | 504 | — | 499 | Input | Input Mix | STD | 0..1 | 1 |
| — | — | — | 500 | Face | Face Track | BOOL | - | 0 |
| — | — | — | 501 | Face | Max Faces | STD | 1..4 | 4 |
| — | — | — | 502 | Face | Face Scale | STD | 0.1..5 | 1.2 |
| — | — | — | 503 | Face | Face Offset X | STD | -1..1 | 0 |
| — | — | — | 504 | Face | Face Offset Y | STD | -1..1 | 0.3 |
| — | — | — | 505 | Face | Follow Roll | STD | 0..1 | 1 |
| — | — | — | 506 | Face | Follow Yaw | STD | 0..1 | 1 |
| — | — | — | 507 | Face | Follow Pitch | STD | 0..1 | 0.6 |
| — | — | — | 508 | Face | Face Smooth | STD | 0..1 | 0.5 |
| — | — | — | 509 | Face | Hide No Face | BOOL | - | 1 |
| 501 | 505 | 496 | 510 | Audio | Audio FFT | BUFFER | 512 bins (FFT) | - |
| 502 | 506 | — | — | Debug | Debug View | OPT | Final / Albedo / Normal / Roughness / Metallic / UV / Part IDs | 0 |
| 503 | 507 | 497 | 511 | Debug | Debug Log | BOOL | - | 0 |

## Standing rules
- COLOURS (W11, Amrit: "always have color use Resolume's built in color picker"): every colour in both bundles is ONE
  Arena colour picker = four CONTIGUOUS params FF_TYPE_HUE, SATURATION, BRIGHTNESS, ALPHA (`MEHR_COLOR` in
  ParamList.inc; read with `Colour()` / `Alpha()`). Proven in Arena 7.28 with `tools/probe`: only the HSBA quartet
  becomes a `[color]` picker; RED/GREEN/BLUE triples AND RED/GREEN/BLUE/ALPHA quartets stay separate plain sliders;
  a lone HUE stays a plain 0..1 slider and does not swallow its neighbours. The picker delivers hue/sat/bri/alpha
  in 0..1 (red = hue 0; wrap hue 1.0 to 0). The ALPHA must DO something: the group's mix/opacity (Tint Mix,
  Background Alpha) or a strength multiplier (lights: intensity; rim, glass tint, holo, pattern, contour, spot, sil).
  Never add a RED/GREEN/BLUE colour again. A HUE that SHIFTS (Hue Shift, Part N Hue Shift, Clone Hue Spread) is not
  a colour and stays a plain slider.
- UNITS (W13; Amrit: "change the dials, not the clip"): every speed is a unit a person can read. Spin, Clone Orbit,
  LowPoly Spin, Tumble X/Y/Z = degrees per second (-30..30). Spot Speed = sweeps per minute (one sweep = there and
  back; 0..15, default 3.75; the old default 0.5 was 15/min). Glitter Twinkle = twinkles per second per flake (0..1,
  default 0.2; own phase and +-20 % rate per flake). Dust Hold = seconds. Hue Drift = turns per second, in true HSB
  (W12: saturation and brightness of the picked colour are kept; the grey-axis rotation is gone).
- Blow To Dust (W13): EVERY press blows; dust already out snaps home and bursts again (`ShapeState::dustRestart`).
  Dust Reform works at any time. The Dust slider scrubs by hand until the next press (a press takes over from it).
- Floor (W13): with a level camera the floor sits right under the mask and its reflection is below the frame, and a
  black shadow over black is invisible. So Floor Camera (default 1) tilts the camera down while Shadow Catcher / Mirror
  Floor are up (+18 deg pitch, target -0.35, distance x1.15, smooth over 0..0.3), and Floor Light (default 0.5) lays a
  key-lit pool on the catcher that the shadow cuts. Floor Height default -1.02 (just under an auto-fitted mask).
- Poses (W13): four default poses ship in Pose Data (front / three-quarter left / low hero / top-down tilt); a slot
  missing from the text keeps its default, Store Pose overwrites one.
- Low Poly facets (W13b): a facet = the rotated-grid cell of a triangle's centroid, computed in the soup vertex stage
  (all three corners agree), so no CPU job; tumble is a quaternion per facet, whole-number speed factors in Separate.
- Texture units (W12): Apple GL 4.1 = 16 samplers per stage, 80 combined. The main FS declares 14; units 17/18 are legal.
- Log (W12): the log is for loads, swaps, eye finding and errors. Every param change, per-drag rebuilds (SHARDS /
  DUST), dust presses, spot / clone speeds and periodic FACE stats are written only while "Debug Log" (Debug) is on.
- RANGE CHANGES (W25, Air Check): Arena's .avc stores a ranged FFGL param as its REAL value + the default, never the range
  (`<ParamRange name="Sim Turbulence" default="0.5" value="2">`), so nothing is remapped proportionally when a range changes.
  A saved value outside the new range is clamped to it (the saved comp holds 0 out-of-range MaskEhr values; L24 C13 read 2.0 =
  the new max). So SHRINKING a range silently clamps every saved clip above the new max, and the next save keeps the clamp.
  Widening a range or moving a default changes no saved clip.
- Regression references (W12 rule, LoadEhr W1 set): verify/w1/ref r1..r6 (our generated models) are pinned by
  tools/regress_refs.sha256 and read-only (MaskEhr W11's "flaky f1" was a reference overwritten by a copy, not the render).
- One Claude on this folder (this repo's own session; MaskEhr is READ ONLY from here). the mask mask: HARD RULE 3. Copies
  preserve timestamps.
- CLOCKS (W14, "no jumps ever"): every integrated phase lives in `LoadEhrPlugin::Clocks` as a double, wrapped to a
  period ALL its readers are seamless across; nothing that reaches a float grows with show time. Periods: angles 360;
  hue / spot sweep 1 turn; Smooth1D is periodic in 200 pi (wobble wraps at 5x, shake at 1x, Random spot at 2x: every
  argument moves by whole periods); smoke drift 100 (noise read at x1, x0.43, x2.63); chrome / dust-wind drift 64
  (`Noise3P` / `NP`: lattice mod 64); glitter 1000 twinkles (per-flake rate rounded to /1000); motes 3600 s (per-mote
  rates rounded to whole cycles per loop; drift = per-mote 0.04 recycle with a seam fade); clone orbit 10 turns (tunnel
  reads x0.3); film swirl 256 and pattern crawl 1000 are not periodic: they cross-fade into (phase - loop) over the last
  5 % (`LoopFade`, `LoopBlend` keeps noise contrast). Tumble = steady turn x (1 - Sway) + Sway x 35 x sin(rock phase),
  both integrated (dragging Sway never jumps). A new clock gets a period + a line in tools/w14_wraps.py. LoadEhr W1: the chain
  clock (`clk.transPlay`): Ping-Pong wraps at 2 x H hops, Loop + Chain Loop at H hops, Loop at the last arrival + its hold; Once clamps.
  The tunnel's ring recycle (front -> back once per turn) is W7 design, not a wrap; W15 hides it (Tunnel Fade).
  W15: Trail Dust grain drift 256 cells (the grain hash is taken mod 256); hue offsets wrap at 1 and glide home through it.
- HUE DRIFT RETURNS HOME (W15, Amrit: "rest should reset"): each hue offset (A, B, C shared by left/right, spots) is
  integrated while its rate is non-zero (|Hue Drift| >= 0.005, the dial shows 0.00 below that) and glides home the short
  way round (smoothstep over "Drift Return", Volume group, 0..3 s, default 0.4; 0 = instant) when it is 0. The picker is
  the anchor. Spot Beat Strobe steps the spot offset a quarter turn x strobe per beat and it returns the same way at 0.
  Log lines (DRIFT ...) only with Debug Log on.
- TUNNEL FADE (W15): tunnel rings dissolve in at the back and out at the front (object-space Fbm dissolve, the W1 noise,
  in the INSTANCED FS; shadow maps and the mirror fade with them). With Depth Fog on, the back end comes out of the fog
  (per-instance fog keep) instead. Per instance: `Instance::keep / fogKeep / age / next` -> `uInstX` / `uInstNext`.
  The instanced programs get `#define INSTANCED` in the FS too; the plain programs' source is unchanged (regress max 0).
- TRAILS (W15): Trail Echoes is a feedback buffer (RGBA16F premultiplied colour + R16F age x coverage, ping-pong, MRT),
  not a snapshot ring: each frame it shifts by Offset/Delay in WHOLE pixels (copies stay crisp), dusty pixels drift,
  pixels blur by Blur x age, age, die at the end of the trail; the live mask is stamped every Delay frames (Smear 0) up
  to every frame with sub-stamps along the shift (Smear 1). A resolve pass (kEchoFS) applies Fade Curve x Decay, the
  dust grain, Color Mix and Glow into echoTex; the composite shader is unchanged. Ghosts share Smear (vertex stretch
  toward the next newer pose: no extra draws; W15 measured sub-ghost instances at 3-4x GPU), Dust, Fade Curve, Glow,
  Color Mix; Blur is echoes only. Ghosts draw with glPolygonOffset(1, 64) so the live surface wins where they meet.
- TRANSITIONS (W16, Amrit: Shatter / Dust "to fast onset ... Dissolve and Slice works very well"): Shatter and Dust
  are slider-driven, no simulation and no clock (scrubbing back retraces; holding still is still). Every piece has an
  order key k (shard: rim first, centre last, jittered; dust point: the dust noise through its CDF MEASURED on the model
  = `DustSlot::keyQ`, 17 quantiles built with the points, so k is even whatever the surface) and flies over
  u = clamp((e - Stagger k) / (1 - Stagger)), e = the slider eased by Trans Ease (smoothstep). Outgoing: away = u;
  incoming: away = 1 - u; path = mix(away, smoothstep(away), Ease); pieces shrink over the far half (x Ease). The
  meshes show exactly the pieces at home (Dust: `TransDustCut` turns the key cut into the noise threshold). The shatter
  transition is its own program pair (`progSoupT/TI`, `#define TRANS`): standalone Shatter compiles exactly as before.
  Trans Ease 0 + Stagger 0 + Spread 1 = the old shatter flight (T = 1.6 s x t). Dissolve / Slice ignore the three dials.
- Continuity: speeds are integrated (spin, wobble); only Reset Spin jumps (by design). Beat Bounce/Nod are
  beat kicks by design (continuous envelope, 0.1-beat attack).
- Handoffs: `docs/handoffs/loadehr_<date>_<slug>.md`, `cp -p` to `~/claude-handoffs/`.

## Reading notes (W3 chain; what the code relies on)
- **Hillaire 2015 (Frostbite volumetrics, SIGGRAPH Advances).** Participating medium = scattering σs + absorption σa,
  extinction σt = σs+σa. Per march step of length dt, in-scattered light S = σs · Σlights(phase · shadow · L).
  Energy-conserving step: `Sint = (S − S·exp(−σt·dt)) / σt; L += T·Sint; T *= exp(−σt·dt)` (the naive `S·dt` gains
  energy at high density). Shadowing per light = a shadow map lookup at the sample. Hide undersampling with a
  jittered start offset per pixel and frame (blue noise / interleaved gradient) + temporal accumulation (~10 % new).
  Transmittance T also gives the medium's own alpha for "Over" compositing.
- **Mitchell, GPU Gems 3 ch.13 (post-process shafts).** From each pixel step toward the light's screen position:
  `delta = (uv − lightUV) · Density / N; decay = 1; for N: uv −= delta; c += occl(uv) · decay · Weight; decay *= Decay;
  out = c · Exposure`. The source frame is an occlusion pre-pass: light colour where the light/sky is visible, black
  where the occluder is. Limits: shafts from behind leak in front of foreground objects; flicker at screen edges;
  light far off-screen / behind the camera breaks it (fade by the light's facing).
- **Henyey-Greenstein:** `p(θ) = (1 − g²) / (4π · (1 + g² − 2g·cosθ)^1.5)`; g>0 forward scatter (bright looking INTO the
  light, i.e. beams toward camera), g<0 back scatter, g=0 isotropic 1/4π.
- **One Euro filter (Casiez 2012):** `α(fc) = 1 / (1 + τ/Te), τ = 1/(2π·fc)`; derivative filtered at dcutoff (1 Hz);
  `fc = minCutoff + β·|dx̂|`. minCutoff lowers jitter at rest, β lowers lag when moving.
- **meshoptimizer (MIT):** `meshopt_simplify(dst, idx, idxCount, pos, vtxCount, stride, targetIdxCount, targetError,
  options, &resultError)` returns the new index count; LODs share the vertex buffer (index buffers only).
- **Apple Vision:** `VNDetectFaceRectanglesRequest` / `VNDetectFaceLandmarksRequest` → `VNFaceObservation` with a
  normalised `boundingBox` (origin bottom-left) and `roll`/`yaw` (macOS 10.13+), `pitch` (macOS 12+). Run on a
  background queue with a `VNImageRequestHandler` over a CVPixelBuffer/CGImage.

## THE COMMONS
Commons: `/Users/amritrosell/Documents/claude/ffgl/_Commons` (repo `AmritusG/ffgl-commons`). LoadEhr is an FFGL repo:
CONTRACT section 1 (Wire/ISF toolchain facts) does not bind it; file only what applies, tagged honestly
(`applies-to: [ffgl]`, `[arena]`, `[isf]`, `[wire]` or `[any]`).
- FIRST, every session, before anything else: read `_Commons/CONTRACT.md`, `_Commons/INDEX.md`, and every finding whose
  tags overlap the wave's work (Claude Code: run `_Commons/tools/commons-sync.sh` before reading, it pulls first).
- `CONTRACT.md` is binding. If it contradicts this repo, STOP and report; propose with a finding tagged
  `contract-proposal`, never edit it.
- Write a finding for each of the five qualifying kinds, every time one occurs: an FFGL / Arena / ISF / Wire behaviour
  another build would hit (host contracts included), a measured cost with its method, a licence ruling, a dead end with
  the reason, a reusable module. Never a design or taste preference.
- New findings use `LOADEHR-<NNN>` (from 001). MaskEhr's are `MASKEHR-*`. Format: `_Commons/PROTOCOL.md`.
- `confidence: measured` requires an Arena FPS number with Wire closed and the machine named (`_Commons/MACHINES.md`).
  Harness ms are `verified` at most (with the method written), else `assumed`.
- Claude Code runs `_Commons/tools/commons-sync.sh` afterwards. Cowork writes the file and stops: Cowork runs no git on
  Amrit's tree.
- Vendored modules (MaskEhr is the origin of these three; LoadEhr's `src/` carries copies, by other names):

| Module | Version | Where in LoadEhr |
|---|---|---|
| hg-phase | v1 | `src/VolumeShaders.h` / `src/ProjectorShaders.h` `HG` |
| blackbody | v1 | `src/Shaders.h` `Blackbody` |
| thin-film-iridescence | v1 | `src/Shaders.h` `ThinFilm` |
