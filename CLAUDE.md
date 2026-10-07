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

LoadEhr is a spinoff of MaskEhr (W1, 2026-10-07, from MaskEhr c7ced33): every MaskEhr feature except the lasers and the library,
plus four model slots that morph into each other. Amrit's words: "I want a version of MaskEhr called LoadEhr that has basically
the same features, no lasers though (might prune more) and that does NOT come with any library and does not HAVE a library. Only
use two import for morphing between loaded 3D models, or, add more loaders to have more morphing capabilities." Four slots =
Cowork's pick under "add, then prune": he may cut to 2. Expect him to prune more after playing with it.

Four bundles from one codebase (`src/LoadEhrPlugin.cpp`, compiled with `LOADEHR_FX=0/1` x `LOADEHR_WOW=0/1`):
- **LoadEhr** (source, ID `LEhr`) — renders any .fbx / .obj / .gltf / .glb over transparent.
- **LoadEhr FX** (effect, ID `LEfx`) — same renderer; the incoming layer can be background, skin or reflection.
- **LoadEhr Wow** (source, `LEwo`) / **LoadEhr Wow FX** (effect, `LEwf`) — the same renderer carrying the W26 showpieces.
IDs checked against every bundle in Extra Effects (2026-10-07: no LEhr / LEfx / LEwo / LEwf anywhere).

BUNDLES: **Resolume shows at most 512 FFGL params per plugin** (Arena drops every param past index 511). One code base, every
param id exists in every build; `MEHR_REGION` blocks in `src/ParamList.inc` say who CARRIES a param: 0 = all, `MEHR_CLASSIC` =
LoadEhr / FX only (Logo, Extrude, Letters, Camera Live, Surface except Glass, Shape except Shatter, Copies, FX, Project Input,
Debug View), `MEHR_WOW` = Wow / Wow FX only (the W26 groups without Lasers, Liquid, Shatter Physics, Spots, Silhouette, and
(LoadEhr W1) Face). A param a bundle does not carry keeps its default (= off). The host sees the carried params as FFGL indices
0..n-1 in id order (`Map()`; the SDK calls are shimmed by id in `LoadEhrPlugin`, the host entry points convert index -> id).
W1: lasers (-24, Wow side) and the library (-2) went out; four slots (+12: Model / Status / Reload x A..D, replacing Model File,
Status, Reload, Model B) and the chain (+3: Chain Loop, Style Order, Hold Time) came in. That put LoadEhr FX at 521, so Face (10,
FX only) moved to the Wow side. Counts: **LoadEhr 507, FX 511, Wow 493, Wow FX 507** (`tools/param_map.py`). Fewest bundles:
the union of the params is ~650 and an effect is a different plugin type than a source, so two carriers per type = four bundles.

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
- Style per hop: Style Order Fixed (Transition Style every hop), Cycle (Transition Style + hop, round the five), Random (a hash
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
- W21: `src/Generate.*` (text / logos -> a generated model, see below), `third_party/nanosvg` (zlib), `third_party/libtess2`
  (SGI Free Software License B 2.0: NOT on Amrit's licence list (MIT / BSD / Apache / zlib / ISC / Boost); open question to him, see
  the W1 handoff). `tools/probe/TextProbe.cpp` = the W21 text-editor probe (not installed). `test_logos/` = OUR generated test
  emblem (`tools/w21_test_logos.py`: SVG + PDF from one geometry, PNG via sips). `tools/w21_arena.py` = set clip params by name over REST.
- W1: `tools/gen_test_models.py` (our test models), `tools/chain_test.py`, `tools/parity.py`, `tools/guards.sh`.
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
  light costs ONE sampler wherever it is read. Picture: FX Proj Source Input (the mip-mapped input copy) or Proj Image (png /
  jpg decoded on a worker thread, <= 2048 px, rows flipped bottom-up, CPU box mips). Beam = kProjMarchFS added (rgb) onto the
  fog's march target before the temporal blend, the ray clipped to the frustum (5 clip-space planes of its VP) so all Vol
  Quality steps land in the beam; the projector alone forces the volume on with `vol.projOnly` (the main march is skipped:
  cleared target). On the mask = the PROJ_PASS variants of the main programs (`progProjMain / Soup / SoupL`, uPLTex0/1 on
  units 22/23: the main FS has 14 samplers + these 2 = 16), drawn additively over the mask (LEQUAL, polygon offset -1/-4),
  only for the plain draw (no clones / Shatter transition; W27: + the mirror room's copies). The plain programs' source is unchanged (#ifdef PROJ_PASS).
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
- Stage 10 INFINITY MIRROR ROOM (group Mirror Room, `src/Mirror.cpp`, `MirrorShaders.h`). InfiniCube's model
  (~/Documents/claude/ffgl/hypercube): bounces UNFOLDED into a lattice of mirrored cells, bounces = |i| + |j| + |k|, tint x
  reflectance per bounce. Here a world-aligned box round the mask (Box Size / Depth, mask units), the front a two-way mirror
  (only cells k <= 0 are seen through it); shapes Box (all six), Corridor (the four upright walls: cells j = 0), Two Mirrors
  (front + back: i = j = 0). The mask's reflections = instanced copies (mirrored on the odd axes, LOD by distance, nearest
  bounces first, <= 192 in batches of 64) through the MIRROR variants (progMainM / SoupM: tint^bounces from Instance::bounces
  in uInstX.w; a fragment shows only if the camera ray to it passes through the box). LED strips = one full-screen pass after
  the composite: the edges of the shape (distance = length of the two smallest per-axis distances to the cell walls),
  sphere-traced from the box entry, dimmed per bounce, hidden by the scene depth; LED Colors Rainbow + a chase (wraps at 1).
  NOT reflected: the volume lights, the
  smoke, fire, lightning, the swarm (they draw once, unmirrored). Cost: skull box +1.1 ms; 719k-tri corridor +6.3 ms (MaskEhr, with
  lasers; tris x the copies).
- W27 MIRROR ROOM FINISHED (`tools/w27_mirror.py`: CLIPS = the 5 Arena clips, TESTS = one look per feature; run / before / pair /
  perf / arena). Arena: layer Wow C22-C26 = Mirror Smoke, Mirror Fire, Mirror Swarm, Mirror Storm, Mirror Projector. Dials (Mirror Room group, Wow only): Mirror Volumes (on), Mirror Vol Steps (4/8/16/32 per bounce, default 8), Mirror
  Particles (on: particles, bolts), Particle Images (1..64 cells, default 24). Room Off = W26 frames (every W26 look
  lands on a W26 outcome; the bimodal ones stay bimodal, Lava Split is multimodal in W26 too).
  * COPIES are shaded where the mask really is: the MIRROR VS folds wpos / normal / tangent back with the instance's fold matrix
    (riding in uInstNext; `Instance::next`), keeps gl_Position unfolded; the FS sees it from the folded camera (`#define uCamPos
    vMCam`) and flips Ng / T / B on odd-bounce copies (an odd reflection turns the winding: gl_FrontFacing reads the other side;
    W26 lit odd copies inside-out and all copies at their unfolded spot). So lights, shadows, env, fire light, flashes are right
    on reflections. Copies also get the projector / caustic picture (progProjMainM / SoupM), the lava glow (progCrackMainM /
    SoupM, unsplit: Lava Split stays on the real mask) and the recorded Bounce flights (progSoupBM).
  * VOLUMES (kMirrorMarchLib): the march's real stretch ends where the ray leaves the box through a mirror (MirrorRange walks
    the wall crossings: stops at Mirror Count bounces, an open side, a +z exit = the two-way front, or the scene depth); the rest
    marches uMSteps = Vol Steps x Count samples, each folded into the box (MirrorFold: density / heat / lights / shadow maps read
    in real space, the ray direction folded for the phase function), light x tint^bounces, coverage x the same. MIRROR variants
    of all five marches (noise, Trip, sim, sim Trip, fire) and of the cookie march (projector beam, caustic smoke). Plain sources
    unchanged (#ifdef MIRROR; `vec3 rd = rdF` shadows the outer ray inside the loop).
  * PARTICLES: fireflies, bolts, motes drawn again per cell (kMirrorPointLib MirrorClip: only what is inside the box,
    only seen through it; uMTint = tint^bounces; motes lit at the real point from the folded camera). Dust (Blow To Dust) is a
    classic-bundle param and never meets the room: not mirrored.
  * Seam (verify/w27/seam): smoke |gradient| on the inner wall seams mean 2.43 / p99 11.9 vs beside them 2.53 / 12.1 (no doubling
    or gaps); smoke images stop at the LED frames for Count 1 / 2 / 4 / 8 (counts.png).
  * NOT reflected: the floor catcher (shadow, mirror floor, caustic floor: composite-pass planes, a floor inside the room is a
    real surface but its image would need the composite to march the lattice), Screen Rays (screen-space: works on the final
    image, no light images), bloom / flares / hue echo / heat haze (screen-space: they include the reflections automatically),
    trails / ghosts / dust (classic bundle), Lava Split (copies stay whole), the Liquid transition and Model B mid-transition
    (copies are drawn only outside transitions, as W26).
  * Cost 1080p (GPU avg, count 3 / 8 / room off): Mirror Smoke 7.4 / 10.3 / 5.5, Mirror Fire 7.7 / 11.0 / 5.7, Mirror Swarm 1.2 /
    1.7 / 0.7, Mirror Storm 3.7 / 6.5 / 2.0, Mirror Projector 4.2 / 6.3 / 2.3. (MaskEhr measurements.)

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
- Extrusion: libtess2 boundary contours (the source fill rule) -> collinear points dropped -> caps; bevel profiles Flat /
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
  keep the wireframe discard at the very end, and take derivatives (`fwidth`) before any discard.
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
index, "—" = not carried). **LoadEhr 507 params, FX 511, Wow 493, Wow FX 507.**
Group order: Model, Text, Logo, Extrude, Letters, Transform, Camera, Camera Live, Motion, Material, Surface, Parts, Shape, Copies, Lighting,
Volume, Smoke Sim, Trip, Fire, Lightning, Swarm, Lava, Caustics, Mirror Room, Light A, Light B, Light C, Spots, Projector, Post, Floor,
Silhouette, FX, Output, Input + Face (FX), Audio, Debug. (W26 also added Liquid to Model and Shatter Physics to Shape.)
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
| 19 | 19 | 19 | 19 | Model | Transition Style | OPT | Dissolve / Shatter / Dust / Slice / Liquid | 0 |
| 20 | 20 | 20 | 20 | Model | Style Order | OPT | Fixed / Cycle / Random | 0 |
| 21 | 21 | 21 | 21 | Model | Transition Play | OPT | Off / Once On Trigger / Ping-Pong / Loop | 0 |
| 22 | 22 | 22 | 22 | Model | Transition Time | STD | 0.5..30 | 6 |
| 23 | 23 | 23 | 23 | Model | Hold Time | STD | 0..60 | 0 |
| 24 | 24 | 24 | 24 | Model | Trans Ease | STD | 0..1 | 1 |
| 25 | 25 | 25 | 25 | Model | Trans Stagger | STD | 0..1 | 0.85 |
| 26 | 26 | 26 | 26 | Model | Trans Spread | STD | 0..4 | 0.6 |
| — | — | 27 | 27 | Model | Liquid Goo | STD | 0..1 | 0.6 |
| — | — | 28 | 28 | Model | Liquid Look | OPT | Chrome / Paint | 0 |
| — | — | 29 | 29 | Model | Liquid Color | HUE | 0..1 | 0.08 |
| — | — | 30 | 30 | Model | Liquid Sat | SAT | 0..1 | 0.6 |
| — | — | 31 | 31 | Model | Liquid Bri | BRI | 0..1 | 1 |
| — | — | 32 | 32 | Model | Liquid Tint Mix | ALPHA | 0..1 | 0 |
| — | — | 33 | 33 | Model | Liquid Wobble | STD | 0..1 | 0.4 |
| — | — | 34 | 34 | Model | Liquid Speed | STD | 0..2 | 0.25 |
| 27 | 27 | 35 | 35 | Text | Text | TEXT | - | - |
| 28 | 28 | 36 | 36 | Text | Font | OPT | Use Font Name / Academy Engraved LET / Al Bayan / Al Nile / Al Tarikh / American Typewriter / Andale Mono / Apple Braille / Apple Chancery / Apple Color Emoji / Apple SD Gothic Neo / Apple Symbols / AppleGothic / AppleMyungjo / Arial / Arial Black / Arial Hebrew / Arial Hebrew Scholar / Arial Narrow / Arial Rounded MT Bold / Arial Unicode MS / Avenir / Avenir Next / Avenir Next Condensed / Ayuthaya / Baghdad / Bangla MN / Bangla Sangam MN / Baskerville / Beirut / Big Caslon / Bodoni 72 / Bodoni 72 Oldstyle / Bodoni 72 Smallcaps / Bodoni Ornaments / Bradley Hand / Brush Script MT / Chalkboard / Chalkboard SE / Chalkduster / Charter / Cochin / Comic Sans MS / Copperplate / Corsiva Hebrew / Courier New / Damascus / DecoType Naskh / Devanagari MT / Devanagari Sangam MN / Didot / DIN Alternate / DIN Condensed / Diwan Kufi / Diwan Thuluth / Euphemia UCAS / Farah / Farisi / Futura / Galvji / GB18030 Bitmap / Geeza Pro / Geneva / Georgia / Gill Sans / Grantha Sangam MN / Gujarati MT / Gujarati Sangam MN / Gurmukhi MN / Gurmukhi MT / Gurmukhi Sangam MN / Heiti SC / Heiti TC / Helvetica / Helvetica Neue / Herculanum / Hiragino Maru Gothic ProN / Hiragino Mincho ProN / Hiragino Sans / Hiragino Sans GB / Hoefler Text / Impact / InaiMathi / ITF Devanagari / ITF Devanagari Marathi / JetBrains Mono / Kailasa / Kannada MN / Kannada Sangam MN / Kefa III / Khmer MN / Khmer Sangam MN / Kohinoor Bangla / Kohinoor Devanagari / Kohinoor Gujarati / Kohinoor Telugu / Kokonor / Krungthep / KufiStandardGK / Lao MN / Lao Sangam MN / Lucida Grande / Luminari / Malayalam MN / Malayalam Sangam MN / Marker Felt / Menlo / Microsoft Sans Serif / Mishafi / Mishafi Gold / Monaco / Mshtakan / Mukta Mahee / Muna / Myanmar MN / Myanmar Sangam MN / Nadeem / New Peninim MT / Noteworthy / Noto Nastaliq Urdu / Noto Sans Batak / Noto Sans Kannada / Noto Sans Myanmar / Noto Sans NKo / Noto Sans Oriya / Noto Sans Syriac / Noto Sans Tagalog / Noto Serif Myanmar / Optima / Oriya MN / Oriya Sangam MN / Palatino / Papyrus / Party LET / Phosphate / PingFang HK / PingFang MO / PingFang SC / PingFang TC / Plantagenet Cherokee / PT Mono / PT Sans / PT Sans Caption / PT Sans Narrow / PT Serif / PT Serif Caption / Raanana / Rockwell / Sana / Sathu / Savoye LET / Shree Devanagari 714 / SignPainter / Silom / Sinhala MN / Sinhala Sangam MN / Skia / Snell Roundhand / Songti SC / Songti TC / STIX Two Math / STIX Two Text / STSong / Studio Manager Big Font / Studio Manager ChFont / Studio Manager Dot Font 1 / Studio Manager Dot Font 3 / Sukhumvit Set / Symbol / Tahoma / Tamil MN / Tamil Sangam MN / Telugu MN / Telugu Sangam MN / Thonburi / Times New Roman / Trattatello / Trebuchet MS / Verdana / Waseem / Webdings / Wingdings / Wingdings 2 / Wingdings 3 / Yamaha EBM 10 / Yamaha EBM 8 / Zapf Dingbats / Zapfino | 0 |
| 29 | 29 | 37 | 37 | Text | Font Name | TEXT | - | - |
| 30 | 30 | 38 | 38 | Text | Font Style | OPT | Default / UltraLight / UltraLight Italic / Thin / Thin Italic / Light / Light Italic / Regular / Italic / Medium / Medium Italic / Bold / Condensed Bold / Bold Italic / Condensed Black | 0 |
| 31 | 31 | 39 | 39 | Text | Font File | FILE | fbx obj gltf glb | - |
| 32 | 32 | 40 | 40 | Text | Align | OPT | Left / Center / Right | 1 |
| 33 | 33 | 41 | 41 | Text | Line Spacing | STD | 0.5..3 | 1 |
| 34 | 34 | 42 | 42 | Text | Letter Spacing | STD | -0.3..2 | 0 |
| 35 | 35 | 43 | 43 | Text | Curve Detail | STD | 0..1 | 0.5 |
| 36 | 36 | — | — | Logo | Logo File | FILE | fbx obj gltf glb | - |
| 37 | 37 | — | — | Logo | Logo Colours | OPT | File Colours / Parts | 0 |
| 38 | 38 | — | — | Logo | Trace Threshold | STD | 0..1 | 0.5 |
| 39 | 39 | — | — | Logo | Trace Smooth | STD | 0..1 | 0.5 |
| 40 | 40 | — | — | Logo | Trace Invert | BOOL | - | 0 |
| 41 | 41 | — | — | Extrude | Extrude Depth | STD | 0.01..2 | 0.25 |
| 42 | 42 | — | — | Extrude | Bevel Size | STD | 0..0.2 | 0.02 |
| 43 | 43 | — | — | Extrude | Bevel Segments | STD | 1..8 | 3 |
| 44 | 44 | — | — | Extrude | Bevel Shape | OPT | Flat / Round / Chamfer / Inset | 1 |
| 45 | 45 | — | — | Extrude | Back Cap | BOOL | - | 1 |
| 46 | 46 | — | — | Extrude | Outline Only | BOOL | - | 0 |
| 47 | 47 | — | — | Extrude | Wall Thickness | STD | 0.005..0.3 | 0.04 |
| 48 | 48 | — | — | Letters | Type On | STD | 0..1 | 1 |
| 49 | 49 | — | — | Letters | Type Speed | STD | 0..30 | 0 |
| 50 | 50 | — | — | Letters | Letter Stagger | STD | 0..1 | 0.5 |
| 51 | 51 | — | — | Letters | Letter Wave | STD | 0..2 | 0 |
| 52 | 52 | — | — | Letters | Wave Speed | STD | 0..5 | 1 |
| 53 | 53 | — | — | Letters | Letter Spin | STD | -30..30 | 0 |
| 54 | 54 | — | — | Letters | Spin Mode | OPT | Together / Separate | 0 |
| 55 | 55 | — | — | Letters | Letter Hop | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| 56 | 56 | — | — | Letters | Hop Height | STD | 0..2 | 0.5 |
| 57 | 57 | 44 | 44 | Transform | Rotate X | STD | -180..180 | 0 |
| 58 | 58 | 45 | 45 | Transform | Rotate Y | STD | -180..180 | 0 |
| 59 | 59 | 46 | 46 | Transform | Rotate Z | STD | -180..180 | 0 |
| 60 | 60 | 47 | 47 | Transform | Spin X | STD | -30..30 | 0 |
| 61 | 61 | 48 | 48 | Transform | Spin Y | STD | -30..30 | 0 |
| 62 | 62 | 49 | 49 | Transform | Spin Z | STD | -30..30 | 0 |
| 63 | 63 | 50 | 50 | Transform | Reset Spin | EVENT | - | - |
| 64 | 64 | 51 | 51 | Transform | Pos X | STD | -3..3 | 0 |
| 65 | 65 | 52 | 52 | Transform | Pos Y | STD | -3..3 | 0 |
| 66 | 66 | 53 | 53 | Transform | Pos Z | STD | -10..10 | 0 |
| 67 | 67 | 54 | 54 | Transform | Scale | STD | 0..6 | 1 |
| 68 | 68 | 55 | 55 | Camera | FOV | STD | 5..150 | 35 |
| 69 | 69 | 56 | 56 | Camera | Distance | STD | 0.3..20 | 4 |
| 70 | 70 | 57 | 57 | Camera | Orbit Yaw | STD | -180..180 | 0 |
| 71 | 71 | 58 | 58 | Camera | Orbit Pitch | STD | -89..89 | 0 |
| 72 | 72 | — | — | Camera Live | Cut Every | OPT | Off / 1 Beat / 2 Beats / 4 Beats / 8 Beats | 0 |
| 73 | 73 | — | — | Camera Live | Cut Angles | STD | 2..8 | 6 |
| 74 | 74 | — | — | Camera Live | Angle Spread | STD | 0..180 | 50 |
| 75 | 75 | — | — | Camera Live | Cut Order | OPT | Random / Sequence | 0 |
| 76 | 76 | — | — | Camera Live | Beat Shake | STD | 0..4 | 0 |
| 77 | 77 | — | — | Camera Live | Shake Decay | STD | 0.02..2 | 0.25 |
| 78 | 78 | — | — | Camera Live | Vertigo | STD | -1..1 | 0 |
| 79 | 79 | — | — | Camera Live | Pose Glide | STD | 0..10 | 1.5 |
| 80 | 80 | — | — | Camera Live | Store Pose 1 | EVENT | - | - |
| 81 | 81 | — | — | Camera Live | Store Pose 2 | EVENT | - | - |
| 82 | 82 | — | — | Camera Live | Store Pose 3 | EVENT | - | - |
| 83 | 83 | — | — | Camera Live | Store Pose 4 | EVENT | - | - |
| 84 | 84 | — | — | Camera Live | Go Pose 1 | EVENT | - | - |
| 85 | 85 | — | — | Camera Live | Go Pose 2 | EVENT | - | - |
| 86 | 86 | — | — | Camera Live | Go Pose 3 | EVENT | - | - |
| 87 | 87 | — | — | Camera Live | Go Pose 4 | EVENT | - | - |
| 88 | 88 | — | — | Camera Live | Pose Data | TEXT (hidden) | - | - |
| 89 | 89 | 59 | 59 | Motion | Wobble Amount | STD | 0..2 | 0 |
| 90 | 90 | 60 | 60 | Motion | Wobble Speed | STD | 0..5 | 0.5 |
| 91 | 91 | 61 | 61 | Motion | Beat Bounce | STD | 0..2 | 0 |
| 92 | 92 | 62 | 62 | Motion | Beat Nod | STD | 0..2 | 0 |
| 93 | 93 | 63 | 63 | Motion | Audio Gain | STD | 0..10 | 1 |
| 94 | 94 | 64 | 64 | Motion | Audio > Scale | STD | 0..3 | 0 |
| 95 | 95 | 65 | 65 | Motion | Audio > Glow | STD | 0..10 | 0 |
| 96 | 96 | 66 | 66 | Motion | Audio > Explode | STD | 0..3 | 0 |
| 97 | 97 | 67 | 67 | Material | Tint Color | HUE | 0..1 | 0 |
| 98 | 98 | 68 | 68 | Material | Tint Sat | SAT | 0..1 | 0 |
| 99 | 99 | 69 | 69 | Material | Tint Bri | BRI | 0..1 | 1 |
| 100 | 100 | 70 | 70 | Material | Tint Mix | ALPHA | 0..1 | 0 |
| 101 | 101 | 71 | 71 | Material | Hue Shift | HUE | 0..1 | 0 |
| 102 | 102 | 72 | 72 | Material | Saturation | STD | 0..4 | 1 |
| 103 | 103 | 73 | 73 | Material | Brightness | STD | 0..6 | 1 |
| 104 | 104 | 74 | 74 | Material | Metal Override | BOOL | - | 0 |
| 105 | 105 | 75 | 75 | Material | Metallic Value | STD | 0..1 | 1 |
| 106 | 106 | 76 | 76 | Material | Roughness Scale | STD | 0..4 | 1 |
| 107 | 107 | 77 | 77 | Material | Normal Strength | STD | 0..6 | 1 |
| 108 | 108 | 78 | 78 | Material | Height Displace | STD | -0.5..0.5 | 0 |
| 109 | 109 | 79 | 79 | Material | Emissive Glow | STD | 0..10 | 1 |
| 110 | 110 | 80 | 80 | Material | Rim Glow | STD | 0..10 | 0 |
| 111 | 111 | 81 | 81 | Material | Rim Color | HUE | 0..1 | 0.104167 |
| 112 | 112 | 82 | 82 | Material | Rim Sat | SAT | 0..1 | 0.4 |
| 113 | 113 | 83 | 83 | Material | Rim Bri | BRI | 0..1 | 1 |
| 114 | 114 | 84 | 84 | Material | Rim Alpha | ALPHA | 0..1 | 1 |
| 115 | 115 | — | — | Surface | Oil Film | STD | 0..2 | 0 |
| 116 | 116 | — | — | Surface | Film Thickness | STD | 0.1..3 | 1 |
| 117 | 117 | — | — | Surface | Film Angle Shift | STD | 0..2 | 1 |
| 118 | 118 | — | — | Surface | Film Swirl | STD | 0..2 | 1 |
| 119 | 119 | — | — | Surface | Glitter | STD | 0..10 | 0 |
| 120 | 120 | — | — | Surface | Glitter Density | STD | 0..1 | 0.35 |
| 121 | 121 | — | — | Surface | Glitter Size | STD | 0.1..6 | 1 |
| 122 | 122 | — | — | Surface | Glitter Twinkle | STD | 0..1 | 0.2 |
| 123 | 123 | — | — | Surface | Glitter On Metal | STD | 0..1 | 0.8 |
| 124 | 124 | — | — | Surface | Liquid Chrome | STD | 0..1 | 0 |
| 125 | 125 | — | — | Surface | Chrome Ripple | STD | 0..3 | 0.6 |
| 126 | 126 | 85 | 85 | Surface | Glass | STD | 0..1 | 0 |
| 127 | 127 | 86 | 86 | Surface | Glass IOR | STD | 1..2.5 | 1.45 |
| 128 | 128 | 87 | 87 | Surface | Glass Dispersion | STD | 0..2 | 0.4 |
| 129 | 129 | 88 | 88 | Surface | Glass Color | HUE | 0..1 | 0.5625 |
| 130 | 130 | 89 | 89 | Surface | Glass Sat | SAT | 0..1 | 0.08 |
| 131 | 131 | 90 | 90 | Surface | Glass Bri | BRI | 0..1 | 1 |
| 132 | 132 | 91 | 91 | Surface | Glass Alpha | ALPHA | 0..1 | 1 |
| 133 | 133 | — | — | Surface | Hologram | STD | 0..1 | 0 |
| 134 | 134 | — | — | Surface | Holo Scanlines | STD | 0..4 | 1 |
| 135 | 135 | — | — | Surface | Holo Flicker | STD | 0..2 | 0.5 |
| 136 | 136 | — | — | Surface | Holo Split | STD | 0..4 | 1 |
| 137 | 137 | — | — | Surface | Holo Glitch | STD | 0..4 | 0 |
| 138 | 138 | — | — | Surface | Holo Color | HUE | 0..1 | 0.52846 |
| 139 | 139 | — | — | Surface | Holo Sat | SAT | 0..1 | 1 |
| 140 | 140 | — | — | Surface | Holo Bri | BRI | 0..1 | 0.762759 |
| 141 | 141 | — | — | Surface | Holo Alpha | ALPHA | 0..1 | 1 |
| 142 | 142 | — | — | Surface | Heat Glow | STD | 0..10 | 0 |
| 143 | 143 | — | — | Surface | Heat Temp | STD | 600..12000 | 1900 |
| 144 | 144 | — | — | Surface | Heat Spread | STD | 0..1 | 0.7 |
| 145 | 145 | — | — | Surface | Pattern | OPT | Off / Stripes / Cells / Noise | 0 |
| 146 | 146 | — | — | Surface | Pattern Amount | STD | 0..10 | 2 |
| 147 | 147 | — | — | Surface | Pattern Scale | STD | 0.2..40 | 5 |
| 148 | 148 | — | — | Surface | Pattern Speed | STD | -4..4 | 0.5 |
| 149 | 149 | — | — | Surface | Pattern Color | HUE | 0..1 | 0.520833 |
| 150 | 150 | — | — | Surface | Pattern Sat | SAT | 0..1 | 0.8 |
| 151 | 151 | — | — | Surface | Pattern Bri | BRI | 0..1 | 1 |
| 152 | 152 | — | — | Surface | Pattern Alpha | ALPHA | 0..1 | 1 |
| 153 | 153 | — | — | Surface | Pattern Beat | STD | 0..4 | 0 |
| 154 | 154 | — | — | Surface | Pattern Audio | STD | 0..4 | 0 |
| 155 | 155 | 92 | 92 | Parts | Part 1 Visible | BOOL | - | 1 |
| 156 | 156 | 93 | 93 | Parts | Part 1 Hue Shift | STD | 0..1 | 0 |
| 157 | 157 | 94 | 94 | Parts | Part 1 Glow | STD | 0..5 | 0 |
| 158 | 158 | 95 | 95 | Parts | Part 1 Skin | BOOL | - | 0 |
| 159 | 159 | 96 | 96 | Parts | Part 1 Surface | STD | 0..1 | 1 |
| 160 | 160 | 97 | 97 | Parts | Part 2 Visible | BOOL | - | 1 |
| 161 | 161 | 98 | 98 | Parts | Part 2 Hue Shift | STD | 0..1 | 0 |
| 162 | 162 | 99 | 99 | Parts | Part 2 Glow | STD | 0..5 | 0 |
| 163 | 163 | 100 | 100 | Parts | Part 2 Skin | BOOL | - | 0 |
| 164 | 164 | 101 | 101 | Parts | Part 2 Surface | STD | 0..1 | 1 |
| 165 | 165 | 102 | 102 | Parts | Part 3 Visible | BOOL | - | 1 |
| 166 | 166 | 103 | 103 | Parts | Part 3 Hue Shift | STD | 0..1 | 0 |
| 167 | 167 | 104 | 104 | Parts | Part 3 Glow | STD | 0..5 | 0 |
| 168 | 168 | 105 | 105 | Parts | Part 3 Skin | BOOL | - | 0 |
| 169 | 169 | 106 | 106 | Parts | Part 3 Surface | STD | 0..1 | 1 |
| 170 | 170 | 107 | 107 | Parts | Part 4 Visible | BOOL | - | 1 |
| 171 | 171 | 108 | 108 | Parts | Part 4 Hue Shift | STD | 0..1 | 0 |
| 172 | 172 | 109 | 109 | Parts | Part 4 Glow | STD | 0..5 | 0 |
| 173 | 173 | 110 | 110 | Parts | Part 4 Skin | BOOL | - | 0 |
| 174 | 174 | 111 | 111 | Parts | Part 4 Surface | STD | 0..1 | 1 |
| 175 | 175 | 112 | 112 | Parts | Part 5 Visible | BOOL | - | 1 |
| 176 | 176 | 113 | 113 | Parts | Part 5 Hue Shift | STD | 0..1 | 0 |
| 177 | 177 | 114 | 114 | Parts | Part 5 Glow | STD | 0..5 | 0 |
| 178 | 178 | 115 | 115 | Parts | Part 5 Skin | BOOL | - | 0 |
| 179 | 179 | 116 | 116 | Parts | Part 5 Surface | STD | 0..1 | 1 |
| 180 | 180 | 117 | 117 | Parts | Part 6 Visible | BOOL | - | 1 |
| 181 | 181 | 118 | 118 | Parts | Part 6 Hue Shift | STD | 0..1 | 0 |
| 182 | 182 | 119 | 119 | Parts | Part 6 Glow | STD | 0..5 | 0 |
| 183 | 183 | 120 | 120 | Parts | Part 6 Skin | BOOL | - | 0 |
| 184 | 184 | 121 | 121 | Parts | Part 6 Surface | STD | 0..1 | 1 |
| 185 | 185 | 122 | 122 | Parts | Part 7 Visible | BOOL | - | 1 |
| 186 | 186 | 123 | 123 | Parts | Part 7 Hue Shift | STD | 0..1 | 0 |
| 187 | 187 | 124 | 124 | Parts | Part 7 Glow | STD | 0..5 | 0 |
| 188 | 188 | 125 | 125 | Parts | Part 7 Skin | BOOL | - | 0 |
| 189 | 189 | 126 | 126 | Parts | Part 7 Surface | STD | 0..1 | 1 |
| 190 | 190 | 127 | 127 | Parts | Part 8 Visible | BOOL | - | 1 |
| 191 | 191 | 128 | 128 | Parts | Part 8 Hue Shift | STD | 0..1 | 0 |
| 192 | 192 | 129 | 129 | Parts | Part 8 Glow | STD | 0..5 | 0 |
| 193 | 193 | 130 | 130 | Parts | Part 8 Skin | BOOL | - | 0 |
| 194 | 194 | 131 | 131 | Parts | Part 8 Surface | STD | 0..1 | 1 |
| 195 | 195 | — | — | Shape | Melt | STD | 0..1 | 0 |
| 196 | 196 | — | — | Shape | Drip | STD | 0..3 | 1 |
| 197 | 197 | 132 | 132 | Shape | Shatter | EVENT | - | - |
| 198 | 198 | 133 | 133 | Shape | Reform | EVENT | - | - |
| 199 | 199 | 134 | 134 | Shape | Shatter On Beat | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| 200 | 200 | 135 | 135 | Shape | Shard Count | STD | 8..512 | 64 |
| 201 | 201 | 136 | 136 | Shape | Shatter Force | STD | 0..10 | 1.5 |
| 202 | 202 | 137 | 137 | Shape | Shatter Gravity | STD | 0..20 | 2 |
| 203 | 203 | 138 | 138 | Shape | Shard Spin | STD | 0..20 | 3 |
| 204 | 204 | 139 | 139 | Shape | Reform Time | STD | 0.05..10 | 1.2 |
| 205 | 205 | — | — | Shape | Slice Count | STD | 2..64 | 12 |
| 206 | 206 | — | — | Shape | Slice Gap | STD | 0..2 | 0 |
| 207 | 207 | — | — | Shape | Slice Axis | OPT | X / Y / Z | 1 |
| 208 | 208 | — | — | Shape | Slice Wave | STD | 0..4 | 0 |
| 209 | 209 | — | — | Shape | Voxel | STD | 0..1 | 0 |
| 210 | 210 | — | — | Shape | Voxel Size | STD | 0.005..0.5 | 0.06 |
| 211 | 211 | — | — | Shape | Low Poly | STD | 0..1 | 0 |
| 212 | 212 | — | — | Shape | Low Poly Size | STD | 0.01..0.8 | 0.15 |
| 213 | 213 | — | — | Shape | LowPoly Rot X | STD | -180..180 | 0 |
| 214 | 214 | — | — | Shape | LowPoly Rot Y | STD | -180..180 | 0 |
| 215 | 215 | — | — | Shape | LowPoly Rot Z | STD | -180..180 | 0 |
| 216 | 216 | — | — | Shape | LowPoly Spin X | STD | -30..30 | 0 |
| 217 | 217 | — | — | Shape | LowPoly Spin Y | STD | -30..30 | 0 |
| 218 | 218 | — | — | Shape | LowPoly Spin Z | STD | -30..30 | 0 |
| 219 | 219 | — | — | Shape | Facet Tumble | OPT | Off / Together / Separate | 0 |
| 220 | 220 | — | — | Shape | Tumble X | STD | -30..30 | 0 |
| 221 | 221 | — | — | Shape | Tumble Y | STD | -30..30 | 0 |
| 222 | 222 | — | — | Shape | Tumble Z | STD | -30..30 | 0 |
| 223 | 223 | — | — | Shape | Tumble Random | STD | 0..1 | 0.5 |
| 224 | 224 | — | — | Shape | Tumble Sway | STD | 0..1 | 0 |
| 225 | 225 | — | — | Shape | Tumble On Beat | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| 226 | 226 | — | — | Shape | Tumble Beat Flip | OPT | 90 / 180 | 0 |
| 227 | 227 | — | — | Shape | Facet Gap | STD | 0..1 | 0 |
| 228 | 228 | — | — | Shape | Tumble Reset | EVENT | - | - |
| 229 | 229 | — | — | Shape | Deform Axis | OPT | X / Y / Z | 1 |
| 230 | 230 | — | — | Shape | Twist | STD | -8..8 | 0 |
| 231 | 231 | — | — | Shape | Bend | STD | -4..4 | 0 |
| 232 | 232 | — | — | Shape | Ripple | STD | 0..0.5 | 0 |
| 233 | 233 | — | — | Shape | Ripple Freq | STD | 1..80 | 14 |
| 234 | 234 | — | — | Shape | Ripple Speed | STD | -20..20 | 4 |
| 235 | 235 | — | — | Shape | Ripple Beat | STD | 0..4 | 0 |
| 236 | 236 | — | — | Shape | Blow To Dust | EVENT | - | - |
| 237 | 237 | — | — | Shape | Dust Reform | EVENT | - | - |
| 238 | 238 | — | — | Shape | Dust On Beat | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| 239 | 239 | — | — | Shape | Dust Hold | STD | 0..30 | 0 |
| 240 | 240 | — | — | Shape | Dust | STD | 0..1 | 0 |
| 241 | 241 | — | — | Shape | Dust Count | OPT | 25k / 50k / 100k / 200k / 400k | 2 |
| 242 | 242 | — | — | Shape | Dust Wind | STD | 0..6 | 1 |
| 243 | 243 | — | — | Shape | Dust Size | STD | 0.3..8 | 1.5 |
| 244 | 244 | — | — | Shape | Contours | STD | 0..10 | 0 |
| 245 | 245 | — | — | Shape | Contour Mode | OPT | X / Y / Z / Height Map / Radial | 1 |
| 246 | 246 | — | — | Shape | Contour Spacing | STD | 0.005..0.5 | 0.06 |
| 247 | 247 | — | — | Shape | Contour Width | STD | 0.01..1 | 0.15 |
| 248 | 248 | — | — | Shape | Contour Speed | STD | -4..4 | 0.3 |
| 249 | 249 | — | — | Shape | Contour Color | HUE | 0..1 | 0.0784314 |
| 250 | 250 | — | — | Shape | Contour Sat | SAT | 0..1 | 0.85 |
| 251 | 251 | — | — | Shape | Contour Bri | BRI | 0..1 | 1 |
| 252 | 252 | — | — | Shape | Contour Alpha | ALPHA | 0..1 | 1 |
| 253 | 253 | — | — | Copies | Clone Layout | OPT | Off / Ring / Grid / Tunnel / Carousel | 0 |
| 254 | 254 | — | — | Copies | Clone Count | STD | 1..64 | 8 |
| 255 | 255 | — | — | Copies | Clone Spacing | STD | 0.2..12 | 2.4 |
| 256 | 256 | — | — | Copies | Clone Spin Step | STD | -180..180 | 0 |
| 257 | 257 | — | — | Copies | Clone Hue Spread | STD | 0..1 | 0 |
| 258 | 258 | — | — | Copies | Clone Orbit | STD | -30..30 | 0 |
| 259 | 259 | — | — | Copies | Clone Scale | STD | 0.05..3 | 0.6 |
| 260 | 260 | — | — | Copies | Tunnel Fade | STD | 0..1 | 0.25 |
| 261 | 261 | — | — | Copies | Keep Original | BOOL | - | 1 |
| 262 | 262 | — | — | Copies | LOD Bias | STD | 0.1..4 | 1 |
| 263 | 263 | — | — | Copies | Trail Echoes | STD | 0..7 | 0 |
| 264 | 264 | — | — | Copies | Trail Delay | STD | 1..30 | 4 |
| 265 | 265 | — | — | Copies | Trail Decay | STD | 0..1 | 0.7 |
| 266 | 266 | — | — | Copies | Trail Offset X | STD | -0.1..0.1 | 0 |
| 267 | 267 | — | — | Copies | Trail Offset Y | STD | -0.1..0.1 | 0 |
| 268 | 268 | — | — | Copies | Ghosts | STD | 0..8 | 0 |
| 269 | 269 | — | — | Copies | Ghost Spacing | STD | 1..30 | 5 |
| 270 | 270 | — | — | Copies | Ghost Fade | STD | 0..1 | 0.6 |
| 271 | 271 | — | — | Copies | Trail Smear | STD | 0..1 | 0 |
| 272 | 272 | — | — | Copies | Trail Blur | STD | 0..2 | 0 |
| 273 | 273 | — | — | Copies | Trail Dust | STD | 0..1 | 0 |
| 274 | 274 | — | — | Copies | Trail Grain | STD | 1..12 | 2.5 |
| 275 | 275 | — | — | Copies | Trail Drift | STD | 0..2 | 0.15 |
| 276 | 276 | — | — | Copies | Trail Drift Dir | STD | -180..180 | 90 |
| 277 | 277 | — | — | Copies | Trail Fade Curve | OPT | Linear / Soft / Long Tail | 0 |
| 278 | 278 | — | — | Copies | Trail Glow | STD | 0..10 | 0 |
| 279 | 279 | — | — | Copies | Trail Color | HUE | 0..1 | 0.55 |
| 280 | 280 | — | — | Copies | Trail Sat | SAT | 0..1 | 0.6 |
| 281 | 281 | — | — | Copies | Trail Bri | BRI | 0..1 | 1 |
| 282 | 282 | — | — | Copies | Trail Alpha | ALPHA | 0..1 | 1 |
| 283 | 283 | — | — | Copies | Trail Color Mix | STD | 0..1 | 0 |
| — | — | 140 | 140 | Shape | Shatter Physics | OPT | Analytic / Bounce | 0 |
| — | — | 141 | 141 | Shape | Bounciness | STD | 0..1 | 0.5 |
| — | — | 142 | 142 | Shape | Floor Friction | STD | 0..1 | 0.3 |
| — | — | 143 | 143 | Shape | Physics Speed | STD | 0..2 | 0.35 |
| 284 | 284 | 144 | 144 | Lighting | Key Azimuth | STD | -180..180 | 35 |
| 285 | 285 | 145 | 145 | Lighting | Key Elevation | STD | -90..90 | 35 |
| 286 | 286 | 146 | 146 | Lighting | Key Intensity | STD | 0..20 | 3 |
| 287 | 287 | 147 | 147 | Lighting | Key Color | HUE | 0..1 | 0.104167 |
| 288 | 288 | 148 | 148 | Lighting | Key Sat | SAT | 0..1 | 0.08 |
| 289 | 289 | 149 | 149 | Lighting | Key Bri | BRI | 0..1 | 1 |
| 290 | 290 | 150 | 150 | Lighting | Key Alpha | ALPHA | 0..1 | 1 |
| 291 | 291 | 151 | 151 | Lighting | Ambient | STD | 0..1 | 0.03 |
| 292 | 292 | 152 | 152 | Lighting | Env Intensity | STD | 0..10 | 1 |
| 293 | 293 | 153 | 153 | Lighting | Env Source | OPT | Studio / Input (FX) / Off | 0 |
| 294 | 294 | 154 | 154 | Lighting | Env Rotate | STD | -180..180 | 0 |
| 295 | 295 | 155 | 155 | Lighting | Exposure | STD | -5..5 | 0 |
| 296 | 296 | 156 | 156 | Lighting | Tonemap | OPT | ACES / Filmic / None | 0 |
| 297 | 297 | 157 | 157 | Volume | Vol Method | OPT | Raymarch / Screen Rays | 0 |
| 298 | 298 | 158 | 158 | Volume | Haze | STD | 0..2 | 0.12 |
| 299 | 299 | 159 | 159 | Volume | Smoke Amount | STD | 0..4 | 1 |
| 300 | 300 | 160 | 160 | Volume | Smoke Scale | STD | 0.1..10 | 1.5 |
| 301 | 301 | 161 | 161 | Volume | Smoke Speed | STD | 0..4 | 0.3 |
| 302 | 302 | 162 | 162 | Volume | Smoke Turbulence | STD | 0..4 | 1 |
| 303 | 303 | 163 | 163 | Volume | Smoke Rise | STD | -2..2 | 0.15 |
| 304 | 304 | 164 | 164 | Volume | Glow Cloud | STD | 0..10 | 1 |
| 305 | 305 | 165 | 165 | Volume | Smoke Ambient | STD | 0..4 | 0 |
| 306 | 306 | 166 | 166 | Volume | Smoke Key Light | STD | 0..4 | 0 |
| 307 | 307 | 167 | 167 | Volume | Volume Size | STD | 0.5..12 | 3.5 |
| 308 | 308 | 168 | 168 | Volume | Scatter G | STD | -0.9..0.99 | 0.6 |
| 309 | 309 | 169 | 169 | Volume | Shaft Length | STD | 0.2..20 | 6 |
| 310 | 310 | 170 | 170 | Volume | Motes | STD | 0..4 | 0 |
| 311 | 311 | 171 | 171 | Volume | Vol Quality | OPT | 16 / 32 / 64 / 128 | 1 |
| 312 | 312 | 172 | 172 | Volume | Vol Res | OPT | Full / Half / Quarter | 1 |
| 313 | 313 | 173 | 173 | Volume | Fog Blend | OPT | Add / Over | 0 |
| 314 | 314 | 174 | 174 | Volume | Drift Return | STD | 0..3 | 0.4 |
| 315 | 315 | 175 | 175 | Smoke Sim | Smoke Engine | OPT | Noise / Sim / Sim + Detail | 0 |
| 316 | 316 | 176 | 176 | Smoke Sim | Sim Res | OPT | 48 / 64 / 96 / 128 | 1 |
| 317 | 317 | 177 | 177 | Smoke Sim | Sim Box | STD | 1..6 | 2.5 |
| 318 | 318 | 178 | 178 | Smoke Sim | Sim Iterations | STD | 10..40 | 20 |
| 319 | 319 | 179 | 179 | Smoke Sim | Buoyancy | STD | 0..10 | 2 |
| 320 | 320 | 180 | 180 | Smoke Sim | Heat Decay | STD | 0..5 | 0.5 |
| 321 | 321 | 181 | 181 | Smoke Sim | Vorticity | STD | 0..10 | 2 |
| 322 | 322 | 182 | 182 | Smoke Sim | Smoke Fade | STD | 0..3 | 0.6 |
| 323 | 323 | 183 | 183 | Smoke Sim | Wind X | STD | -3..3 | 0 |
| 324 | 324 | 184 | 184 | Smoke Sim | Wind Y | STD | -3..3 | 0 |
| 325 | 325 | 185 | 185 | Smoke Sim | Wind Z | STD | -3..3 | 0 |
| 326 | 326 | 186 | 186 | Smoke Sim | Sim Turbulence | STD | 0..2 | 0.5 |
| 327 | 327 | 187 | 187 | Smoke Sim | Emit Eyes | STD | 0..10 | 0 |
| 328 | 328 | 188 | 188 | Smoke Sim | Eyes Heat | STD | 0..5 | 1 |
| 329 | 329 | 189 | 189 | Smoke Sim | Emit Behind | STD | 0..10 | 0 |
| 330 | 330 | 190 | 190 | Smoke Sim | Behind Heat | STD | 0..5 | 1 |
| 331 | 331 | 191 | 191 | Smoke Sim | Emit Rim | STD | 0..10 | 0 |
| 332 | 332 | 192 | 192 | Smoke Sim | Rim Heat | STD | 0..5 | 1 |
| 333 | 333 | 193 | 193 | Smoke Sim | Emit Inside | STD | 0..10 | 0 |
| 334 | 334 | 194 | 194 | Smoke Sim | Inside Heat | STD | 0..5 | 1 |
| 335 | 335 | 195 | 195 | Smoke Sim | Emit Offset | STD | 0..0.5 | 0 |
| 336 | 336 | 196 | 196 | Smoke Sim | Beat Puff | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| 337 | 337 | 197 | 197 | Smoke Sim | Puff Amount | STD | 0..10 | 3 |
| 338 | 338 | 198 | 198 | Smoke Sim | Audio > Emit | STD | 0..4 | 0 |
| 339 | 339 | 199 | 199 | Smoke Sim | Sim Reset | EVENT | - | - |
| 340 | 340 | 200 | 200 | Smoke Sim | Smoke Color | HUE | 0..1 | 0 |
| 341 | 341 | 201 | 201 | Smoke Sim | Smoke Sat | SAT | 0..1 | 0 |
| 342 | 342 | 202 | 202 | Smoke Sim | Smoke Bri | BRI | 0..1 | 1 |
| 343 | 343 | 203 | 203 | Smoke Sim | Smoke Alpha | ALPHA | 0..1 | 1 |
| 344 | 344 | 204 | 204 | Smoke Sim | Smoke Confine | STD | 0..1 | 0 |
| — | 345 | — | — | Smoke Sim | Project Input | STD | 0..1 | 0 |
| 345 | 346 | 205 | 205 | Trip | Trip Master | STD | 0..2 | 1 |
| 346 | 347 | 206 | 206 | Trip | Colour By | OPT | Off / Density / Heat / Speed / Swirl / Age / Height | 0 |
| 347 | 348 | 207 | 207 | Trip | Trip Palette | OPT | Rainbow / Acid / Neon / Fire / Ice / Picker Pair | 0 |
| 348 | 349 | 208 | 208 | Trip | Palette Shift | STD | -2..2 | 0 |
| 349 | 350 | 209 | 209 | Trip | Palette Spread | STD | 0..4 | 1 |
| 350 | 351 | 210 | 210 | Trip | Trip A Color | HUE | 0..1 | 0.83 |
| 351 | 352 | 211 | 211 | Trip | Trip A Sat | SAT | 0..1 | 1 |
| 352 | 353 | 212 | 212 | Trip | Trip A Bri | BRI | 0..1 | 1 |
| 353 | 354 | 213 | 213 | Trip | Trip A Alpha | ALPHA | 0..1 | 1 |
| 354 | 355 | 214 | 214 | Trip | Trip B Color | HUE | 0..1 | 0.45 |
| 355 | 356 | 215 | 215 | Trip | Trip B Sat | SAT | 0..1 | 1 |
| 356 | 357 | 216 | 216 | Trip | Trip B Bri | BRI | 0..1 | 1 |
| 357 | 358 | 217 | 217 | Trip | Trip B Alpha | ALPHA | 0..1 | 1 |
| 358 | 359 | 218 | 218 | Trip | Thin Film | STD | 0..2 | 0 |
| 359 | 360 | 219 | 219 | Trip | Thin Film Thick | STD | 0.1..3 | 1 |
| 360 | 361 | 220 | 220 | Trip | Prism | STD | 0..2 | 0 |
| 361 | 362 | 221 | 221 | Trip | Prism Angle | STD | -180..180 | 0 |
| 362 | 363 | 222 | 222 | Trip | Kaleido Smoke | STD | 1..16 | 1 |
| 363 | 364 | 223 | 223 | Trip | Kaleido Twist | STD | -360..360 | 0 |
| 364 | 365 | 224 | 224 | Trip | Kaleido Spin | STD | -30..30 | 0 |
| 365 | 366 | 225 | 225 | Trip | Fractal Fog | STD | 0..2 | 0 |
| 366 | 367 | 226 | 226 | Trip | Fractal Scale | STD | 0.2..4 | 1 |
| 367 | 368 | 227 | 227 | Trip | Fractal Iter | STD | 1..6 | 3 |
| 368 | 369 | 228 | 228 | Trip | Fractal Morph | STD | -30..30 | 6 |
| 369 | 370 | 229 | 229 | Trip | Neon Veins | STD | 0..10 | 0 |
| 370 | 371 | 230 | 230 | Trip | Vein Width | STD | 0..1 | 0.4 |
| 371 | 372 | 231 | 231 | Trip | Vein Color | HUE | 0..1 | 0.5 |
| 372 | 373 | 232 | 232 | Trip | Vein Sat | SAT | 0..1 | 1 |
| 373 | 374 | 233 | 233 | Trip | Vein Bri | BRI | 0..1 | 1 |
| 374 | 375 | 234 | 234 | Trip | Vein Alpha | ALPHA | 0..1 | 1 |
| 375 | 376 | 235 | 235 | Trip | Smoke Contours | STD | 0..10 | 0 |
| 376 | 377 | 236 | 236 | Trip | Contour Bands | STD | 2..32 | 8 |
| 377 | 378 | 237 | 237 | Trip | Band Sharpness | STD | 0..1 | 0.6 |
| 378 | 379 | 238 | 238 | Trip | Band Scroll | STD | -4..4 | 0.3 |
| 379 | 380 | 239 | 239 | Trip | Contour Style | OPT | Lines / Bands | 0 |
| 380 | 381 | 240 | 240 | Trip | Line Width | STD | 0.5..6 | 1.5 |
| 381 | 382 | 241 | 241 | Trip | Line Glow | STD | 0..4 | 1 |
| 382 | 383 | 242 | 242 | Trip | Blacklight | STD | 0..10 | 0 |
| 383 | 384 | 243 | 243 | Trip | Blacklight By | OPT | Density / Heat | 0 |
| 384 | 385 | 244 | 244 | Trip | Blacklight Color | HUE | 0..1 | 0.76 |
| 385 | 386 | 245 | 245 | Trip | Blacklight Sat | SAT | 0..1 | 0.85 |
| 386 | 387 | 246 | 246 | Trip | Blacklight Bri | BRI | 0..1 | 1 |
| 387 | 388 | 247 | 247 | Trip | Blacklight Alpha | ALPHA | 0..1 | 1 |
| 388 | 389 | 248 | 248 | Trip | Fire Ring | EVENT | - | - |
| 389 | 390 | 249 | 249 | Trip | Ring On Beat | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| 390 | 391 | 250 | 250 | Trip | Ring From | OPT | Eyes / Mouth / Behind | 0 |
| 391 | 392 | 251 | 251 | Trip | Ring Size | STD | 0.05..1 | 0.3 |
| 392 | 393 | 252 | 252 | Trip | Ring Speed | STD | 0..6 | 2 |
| 393 | 394 | 253 | 253 | Trip | Ring Spin | STD | -10..10 | 0 |
| 394 | 395 | 254 | 254 | Trip | Ring Smoke | STD | 0..10 | 4 |
| 395 | 396 | 255 | 255 | Trip | Smoke Ghosts | STD | 0..4 | 0 |
| 396 | 397 | 256 | 256 | Trip | Ghost Offset | STD | 0..3 | 1 |
| 397 | 398 | 257 | 257 | Trip | Ghost Scale | STD | 0.3..3 | 1.3 |
| 398 | 399 | 258 | 258 | Trip | Ghost Fade Time | STD | 0.1..5 | 1.5 |
| 399 | 400 | 259 | 259 | Trip | Whirlpool | STD | -5..5 | 0 |
| 400 | 401 | 260 | 260 | Trip | Hyperspace | STD | -5..5 | 0 |
| 401 | 402 | 261 | 261 | Trip | Hue Echo | STD | 0..1 | 0 |
| 402 | 403 | 262 | 262 | Trip | Echo Hue Step | STD | -0.1..0.1 | 0.02 |
| 403 | 404 | 263 | 263 | Trip | Time Warp | STD | 0..2 | 1 |
| 404 | 405 | 264 | 264 | Trip | Freeze | EVENT | - | - |
| — | — | 265 | 265 | Fire | Fire | STD | 0..1 | 0 |
| — | — | 266 | 266 | Fire | Fire Speed | STD | 0..2 | 0.5 |
| — | — | 267 | 267 | Fire | Burn Rate | STD | 0.2..10 | 2 |
| — | — | 268 | 268 | Fire | Flame Temp | STD | 1000..6000 | 2000 |
| — | — | 269 | 269 | Fire | Flame Height | STD | 0..4 | 1 |
| — | — | 270 | 270 | Fire | Flame Glow | STD | 0..10 | 2 |
| — | — | 271 | 271 | Fire | Fire Smoke | STD | 0..2 | 0.4 |
| — | — | 272 | 272 | Fire | Flame Tint Color | HUE | 0..1 | 0.6 |
| — | — | 273 | 273 | Fire | Flame Tint Sat | SAT | 0..1 | 0.8 |
| — | — | 274 | 274 | Fire | Flame Tint Bri | BRI | 0..1 | 1 |
| — | — | 275 | 275 | Fire | Flame Tint Mix | ALPHA | 0..1 | 0 |
| — | — | 276 | 276 | Fire | Fire Burst | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| — | — | 277 | 277 | Fire | Burst Amount | STD | 0..10 | 2 |
| — | — | 278 | 278 | Fire | Fire Light | STD | 0..10 | 2 |
| — | — | 279 | 279 | Lightning | Lightning Strike | EVENT | - | - |
| — | — | 280 | 280 | Lightning | Strike On Beat | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| — | — | 281 | 281 | Lightning | Strike Rate | STD | 0..10 | 0 |
| — | — | 282 | 282 | Lightning | Bolt Path | OPT | Crawl / Eye To Eye / To Floor / To Clones | 0 |
| — | — | 283 | 283 | Lightning | Bolt Count | STD | 1..8 | 2 |
| — | — | 284 | 284 | Lightning | Branching | STD | 0..1 | 0.5 |
| — | — | 285 | 285 | Lightning | Jaggedness | STD | 0..1 | 0.5 |
| — | — | 286 | 286 | Lightning | Bolt Life | STD | 0.05..2 | 0.4 |
| — | — | 287 | 287 | Lightning | Bolt Width | STD | 0.5..6 | 1.5 |
| — | — | 288 | 288 | Lightning | Bolt Glow | STD | 0..10 | 3 |
| — | — | 289 | 289 | Lightning | Bolt Color | HUE | 0..1 | 0.64 |
| — | — | 290 | 290 | Lightning | Bolt Sat | SAT | 0..1 | 0.55 |
| — | — | 291 | 291 | Lightning | Bolt Bri | BRI | 0..1 | 1 |
| — | — | 292 | 292 | Lightning | Bolt Alpha | ALPHA | 0..1 | 1 |
| — | — | 293 | 293 | Lightning | Bolt Flash | STD | 0..10 | 2 |
| — | — | 294 | 294 | Swarm | Swarm Count | OPT | Off / 2k / 5k / 10k / 20k / 40k | 0 |
| — | — | 295 | 295 | Swarm | Swarm Release | EVENT | - | - |
| — | — | 296 | 296 | Swarm | Release On Beat | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| — | — | 297 | 297 | Swarm | Release Time | STD | 0.2..10 | 2 |
| — | — | 298 | 298 | Swarm | Attraction | STD | 0..10 | 3 |
| — | — | 299 | 299 | Swarm | Flock | STD | 0..2 | 1 |
| — | — | 300 | 300 | Swarm | Swarm Speed | STD | 0..4 | 1 |
| — | — | 301 | 301 | Swarm | Swarm Offset | STD | 0..1 | 0.03 |
| — | — | 302 | 302 | Swarm | Firefly Size | STD | 0.5..8 | 2 |
| — | — | 303 | 303 | Swarm | Firefly Glow | STD | 0..10 | 3 |
| — | — | 304 | 304 | Swarm | Twinkle | STD | 0..1 | 0.5 |
| — | — | 305 | 305 | Swarm | Firefly Color | HUE | 0..1 | 0.14 |
| — | — | 306 | 306 | Swarm | Firefly Sat | SAT | 0..1 | 0.75 |
| — | — | 307 | 307 | Swarm | Firefly Bri | BRI | 0..1 | 1 |
| — | — | 308 | 308 | Swarm | Firefly Alpha | ALPHA | 0..1 | 1 |
| — | — | 309 | 309 | Swarm | Mask Visible | BOOL | - | 1 |
| — | — | 310 | 310 | Lava | Lava Cracks | STD | 0..1 | 0 |
| — | — | 311 | 311 | Lava | Crack Spread | STD | 0..1 | 1 |
| — | — | 312 | 312 | Lava | Crack Start | EVENT | - | - |
| — | — | 313 | 313 | Lava | Crack Reverse | EVENT | - | - |
| — | — | 314 | 314 | Lava | Crack On Beat | OPT | Off / Every Beat / Every 2 / Every 4 / Every 8 | 0 |
| — | — | 315 | 315 | Lava | Crack Time | STD | 0.2..10 | 2 |
| — | — | 316 | 316 | Lava | Crack Scale | STD | 0.5..8 | 2.5 |
| — | — | 317 | 317 | Lava | Crack Width | STD | 0..1 | 0.3 |
| — | — | 318 | 318 | Lava | Lava Temp | STD | 800..4000 | 1600 |
| — | — | 319 | 319 | Lava | Lava Glow | STD | 0..10 | 3 |
| — | — | 320 | 320 | Lava | Lava Tint Color | HUE | 0..1 | 0.55 |
| — | — | 321 | 321 | Lava | Lava Tint Sat | SAT | 0..1 | 0.8 |
| — | — | 322 | 322 | Lava | Lava Tint Bri | BRI | 0..1 | 1 |
| — | — | 323 | 323 | Lava | Lava Tint Mix | ALPHA | 0..1 | 0 |
| — | — | 324 | 324 | Lava | Heat Haze | STD | 0..2 | 0.5 |
| — | — | 325 | 325 | Lava | Lava Split | STD | 0..1 | 0 |
| — | — | 326 | 326 | Lava | Lava Light | STD | 0..40 | 10 |
| — | — | 327 | 327 | Caustics | Caustics | STD | 0..4 | 0 |
| — | — | 328 | 328 | Caustics | Caustic Source | OPT | Glass Parts / Whole Mask | 0 |
| — | — | 329 | 329 | Caustics | Caustic IOR | STD | 1..2.5 | 1.5 |
| — | — | 330 | 330 | Caustics | Caustic Rainbow | STD | 0..2 | 0.5 |
| — | — | 331 | 331 | Caustics | Caustic Sharp | STD | 0..1 | 0.6 |
| — | — | 332 | 332 | Caustics | Caustic Ripple | STD | 0..1 | 0.3 |
| — | — | 333 | 333 | Caustics | Caustic Floor | STD | 0..2 | 1 |
| — | — | 334 | 334 | Caustics | Caustic Smoke | STD | 0..2 | 0.5 |
| — | — | 335 | 335 | Caustics | Caustic Surface | STD | 0..2 | 1 |
| — | — | 336 | 336 | Caustics | Caustic Color | HUE | 0..1 | 0.12 |
| — | — | 337 | 337 | Caustics | Caustic Sat | SAT | 0..1 | 0.15 |
| — | — | 338 | 338 | Caustics | Caustic Bri | BRI | 0..1 | 1 |
| — | — | 339 | 339 | Caustics | Caustic Alpha | ALPHA | 0..1 | 1 |
| — | — | 340 | 340 | Mirror Room | Mirror Room | OPT | Off / Box / Corridor / Two Mirrors | 0 |
| — | — | 341 | 341 | Mirror Room | Mirror Count | STD | 1..8 | 3 |
| — | — | 342 | 342 | Mirror Room | Box Size | STD | 0.8..6 | 1.6 |
| — | — | 343 | 343 | Mirror Room | Box Depth | STD | 0.5..6 | 1.6 |
| — | — | 344 | 344 | Mirror Room | Mirror Color | HUE | 0..1 | 0.55 |
| — | — | 345 | 345 | Mirror Room | Mirror Sat | SAT | 0..1 | 0.12 |
| — | — | 346 | 346 | Mirror Room | Mirror Bri | BRI | 0..1 | 1 |
| — | — | 347 | 347 | Mirror Room | Mirror Reflect | ALPHA | 0..1 | 0.8 |
| — | — | 348 | 348 | Mirror Room | Mirror Gap | STD | 0..4 | 1 |
| — | — | 349 | 349 | Mirror Room | LED Width | STD | 0.2..4 | 1 |
| — | — | 350 | 350 | Mirror Room | LED Colors | OPT | Picker / Rainbow | 0 |
| — | — | 351 | 351 | Mirror Room | LED Color | HUE | 0..1 | 0.58 |
| — | — | 352 | 352 | Mirror Room | LED Sat | SAT | 0..1 | 0.3 |
| — | — | 353 | 353 | Mirror Room | LED Bri | BRI | 0..1 | 1 |
| — | — | 354 | 354 | Mirror Room | LED Alpha | ALPHA | 0..1 | 1 |
| — | — | 355 | 355 | Mirror Room | LED Speed | STD | -4..4 | 0.3 |
| — | — | 356 | 356 | Mirror Room | Mirror Volumes | BOOL | - | 1 |
| — | — | 357 | 357 | Mirror Room | Mirror Vol Steps | OPT | 4 / 8 / 16 / 32 | 1 |
| — | — | 358 | 358 | Mirror Room | Mirror Particles | BOOL | - | 1 |
| — | — | 359 | 359 | Mirror Room | Particle Images | STD | 1..64 | 24 |
| 405 | 406 | 360 | 360 | Light A | A Enable | BOOL | - | 0 |
| 406 | 407 | 361 | 361 | Light A | A Type | OPT | Spot / Point | 0 |
| 407 | 408 | 362 | 362 | Light A | A Anchor To Mask | BOOL | - | 1 |
| 408 | 409 | 363 | 363 | Light A | A Pos X | STD | -6..6 | 0 |
| 409 | 410 | 364 | 364 | Light A | A Pos Y | STD | -6..6 | 0 |
| 410 | 411 | 365 | 365 | Light A | A Pos Z | STD | -6..6 | -1.5 |
| 411 | 412 | 366 | 366 | Light A | A Aim Mode | OPT | At Camera / At Mask / Free | 0 |
| 412 | 413 | 367 | 367 | Light A | A Aim Yaw | STD | -180..180 | 0 |
| 413 | 414 | 368 | 368 | Light A | A Aim Pitch | STD | -90..90 | 0 |
| 414 | 415 | 369 | 369 | Light A | A Cone Angle | STD | 1..170 | 45 |
| 415 | 416 | 370 | 370 | Light A | A Cone Soft | STD | 0..1 | 0.5 |
| 416 | 417 | 371 | 371 | Light A | A Intensity | STD | 0..100 | 12 |
| 417 | 418 | 372 | 372 | Light A | A Color | HUE | 0..1 | 0.105263 |
| 418 | 419 | 373 | 373 | Light A | A Sat | SAT | 0..1 | 0.38 |
| 419 | 420 | 374 | 374 | Light A | A Bri | BRI | 0..1 | 1 |
| 420 | 421 | 375 | 375 | Light A | A Alpha | ALPHA | 0..1 | 1 |
| 421 | 422 | 376 | 376 | Light A | A Hue Drift | STD | -0.7..0.7 | 0 |
| 422 | 423 | 377 | 377 | Light A | A Beat Pulse | STD | 0..4 | 0 |
| 423 | 424 | 378 | 378 | Light A | A Audio > Light | STD | 0..4 | 0 |
| 424 | 425 | 379 | 379 | Light B | B Enable | BOOL | - | 0 |
| 425 | 426 | 380 | 380 | Light B | B Type | OPT | Spot / Point | 0 |
| 426 | 427 | 381 | 381 | Light B | B Anchor To Mask | BOOL | - | 0 |
| 427 | 428 | 382 | 382 | Light B | B Pos X | STD | -6..6 | 2.5 |
| 428 | 429 | 383 | 383 | Light B | B Pos Y | STD | -6..6 | 0.6 |
| 429 | 430 | 384 | 384 | Light B | B Pos Z | STD | -6..6 | 1 |
| 430 | 431 | 385 | 385 | Light B | B Aim Mode | OPT | At Camera / At Mask / Free | 1 |
| 431 | 432 | 386 | 386 | Light B | B Aim Yaw | STD | -180..180 | 0 |
| 432 | 433 | 387 | 387 | Light B | B Aim Pitch | STD | -90..90 | 0 |
| 433 | 434 | 388 | 388 | Light B | B Cone Angle | STD | 1..170 | 45 |
| 434 | 435 | 389 | 389 | Light B | B Cone Soft | STD | 0..1 | 0.5 |
| 435 | 436 | 390 | 390 | Light B | B Intensity | STD | 0..100 | 12 |
| 436 | 437 | 391 | 391 | Light B | B Color | HUE | 0..1 | 0.592593 |
| 437 | 438 | 392 | 392 | Light B | B Sat | SAT | 0..1 | 0.45 |
| 438 | 439 | 393 | 393 | Light B | B Bri | BRI | 0..1 | 1 |
| 439 | 440 | 394 | 394 | Light B | B Alpha | ALPHA | 0..1 | 1 |
| 440 | 441 | 395 | 395 | Light B | B Hue Drift | STD | -0.7..0.7 | 0 |
| 441 | 442 | 396 | 396 | Light B | B Beat Pulse | STD | 0..4 | 0 |
| 442 | 443 | 397 | 397 | Light B | B Audio > Light | STD | 0..4 | 0 |
| 443 | 444 | 398 | 398 | Light C | C Enable | BOOL | - | 0 |
| 444 | 445 | 399 | 399 | Light C | C Place At Eyes | EVENT | - | - |
| 445 | 446 | 400 | 400 | Light C | C Fit Cone | EVENT | - | - |
| 446 | 447 | 401 | 401 | Light C | C Front Only | BOOL | - | 1 |
| 447 | 448 | 402 | 402 | Light C | C Behind Depth | STD | 0.02..2 | 0.3 |
| 448 | 449 | 403 | 403 | Light C | C Anchor To Mask | BOOL | - | 1 |
| 449 | 450 | 404 | 404 | Light C | C Mirror X | BOOL | - | 1 |
| 450 | 451 | 405 | 405 | Light C | C Left Pos X | STD | -6..6 | -0.3 |
| 451 | 452 | 406 | 406 | Light C | C Left Pos Y | STD | -6..6 | 0.2 |
| 452 | 453 | 407 | 407 | Light C | C Left Pos Z | STD | -6..6 | 0.2 |
| 453 | 454 | 408 | 408 | Light C | C Right Pos X | STD | -6..6 | 0.3 |
| 454 | 455 | 409 | 409 | Light C | C Right Pos Y | STD | -6..6 | 0.2 |
| 455 | 456 | 410 | 410 | Light C | C Right Pos Z | STD | -6..6 | 0.2 |
| 456 | 457 | 411 | 411 | Light C | C Aim Mode | OPT | Through Hole / At Camera / Free | 0 |
| 457 | 458 | 412 | 412 | Light C | C Aim Yaw | STD | -180..180 | 0 |
| 458 | 459 | 413 | 413 | Light C | C Aim Pitch | STD | -90..90 | 0 |
| 459 | 460 | 414 | 414 | Light C | C Cone Angle | STD | 1..170 | 20 |
| 460 | 461 | 415 | 415 | Light C | C Cone Soft | STD | 0..1 | 0.5 |
| 461 | 462 | 416 | 416 | Light C | C Intensity | STD | 0..100 | 6 |
| 462 | 463 | 417 | 417 | Light C | C Left Color | HUE | 0..1 | 0.105263 |
| 463 | 464 | 418 | 418 | Light C | C Left Sat | SAT | 0..1 | 0.38 |
| 464 | 465 | 419 | 419 | Light C | C Left Bri | BRI | 0..1 | 1 |
| 465 | 466 | 420 | 420 | Light C | C Left Alpha | ALPHA | 0..1 | 1 |
| 466 | 467 | 421 | 421 | Light C | C Link Colors | BOOL | - | 1 |
| 467 | 468 | 422 | 422 | Light C | C Right Color | HUE | 0..1 | 0.105263 |
| 468 | 469 | 423 | 423 | Light C | C Right Sat | SAT | 0..1 | 0.38 |
| 469 | 470 | 424 | 424 | Light C | C Right Bri | BRI | 0..1 | 1 |
| 470 | 471 | 425 | 425 | Light C | C Right Alpha | ALPHA | 0..1 | 1 |
| 471 | 472 | 426 | 426 | Light C | C Hue Drift | STD | -0.7..0.7 | 0 |
| 472 | 473 | 427 | 427 | Light C | C Beat Pulse | STD | 0..4 | 0 |
| 473 | 474 | 428 | 428 | Light C | C Audio > Light | STD | 0..4 | 0 |
| 474 | 475 | 429 | 429 | Light C | C Placed For | TEXT (hidden) | - | - |
| — | — | 430 | 430 | Spots | Spot Count | OPT | Off / 1 / 2 / 3 / 4 | 0 |
| — | — | 431 | 431 | Spots | Spot Intensity | STD | 0..100 | 25 |
| — | — | 432 | 432 | Spots | Spot Cone | STD | 2..90 | 14 |
| — | — | 433 | 433 | Spots | Spot Speed | STD | 0..15 | 3.75 |
| — | — | 434 | 434 | Spots | Spot Pattern | OPT | Pan / Circle / Figure 8 / Cross / Random | 0 |
| — | — | 435 | 435 | Spots | Spot Width | STD | 0..4 | 1.2 |
| — | — | 436 | 436 | Spots | Spot Rig Height | STD | -4..6 | 2.5 |
| — | — | 437 | 437 | Spots | Spot Rig Dist | STD | 0.5..10 | 3 |
| — | — | 438 | 438 | Spots | Spot Color | HUE | 0..1 | 0 |
| — | — | 439 | 439 | Spots | Spot Sat | SAT | 0..1 | 0 |
| — | — | 440 | 440 | Spots | Spot Bri | BRI | 0..1 | 1 |
| — | — | 441 | 441 | Spots | Spot Alpha | ALPHA | 0..1 | 1 |
| — | — | 442 | 442 | Spots | Spot Color Mode | OPT | Single / Rainbow / Pairs | 0 |
| — | — | 443 | 443 | Spots | Spot Beat Strobe | STD | 0..1 | 0 |
| — | — | 444 | 444 | Spots | Spot Beams | STD | 0..4 | 1 |
| — | — | 445 | 445 | Projector | Proj Enable | BOOL | - | 0 |
| — | — | — | 446 | Projector | Proj Source | OPT | Input / Image File | 0 |
| — | — | 446 | 447 | Projector | Proj Image | FILE | fbx obj gltf glb | - |
| — | — | 447 | 448 | Projector | Proj Anchor | BOOL | - | 0 |
| — | — | 448 | 449 | Projector | Proj Pos X | STD | -6..6 | 0.8 |
| — | — | 449 | 450 | Projector | Proj Pos Y | STD | -6..6 | 1.2 |
| — | — | 450 | 451 | Projector | Proj Pos Z | STD | -6..6 | 4 |
| — | — | 451 | 452 | Projector | Proj Aim Mode | OPT | At Mask / Free | 0 |
| — | — | 452 | 453 | Projector | Proj Aim Yaw | STD | -180..180 | 0 |
| — | — | 453 | 454 | Projector | Proj Aim Pitch | STD | -90..90 | 0 |
| — | — | 454 | 455 | Projector | Proj Throw | STD | 5..120 | 30 |
| — | — | 455 | 456 | Projector | Proj Focus | STD | 0..1 | 0 |
| — | — | 456 | 457 | Projector | Proj Intensity | STD | 0..100 | 25 |
| — | — | 457 | 458 | Projector | Proj Color | HUE | 0..1 | 0 |
| — | — | 458 | 459 | Projector | Proj Sat | SAT | 0..1 | 0 |
| — | — | 459 | 460 | Projector | Proj Bri | BRI | 0..1 | 1 |
| — | — | 460 | 461 | Projector | Proj Alpha | ALPHA | 0..1 | 1 |
| — | — | 461 | 462 | Projector | Proj Beam | STD | 0..4 | 1 |
| — | — | 462 | 463 | Projector | Proj Surface | STD | 0..4 | 1 |
| 475 | 476 | 463 | 464 | Post | Bloom | STD | 0..10 | 0 |
| 476 | 477 | 464 | 465 | Post | Bloom Threshold | STD | 0..1 | 0.65 |
| 477 | 478 | 465 | 466 | Post | Bloom Radius | STD | 0..1 | 0.6 |
| 478 | 479 | 466 | 467 | Post | Flare Ghosts | STD | 0..10 | 0 |
| 479 | 480 | 467 | 468 | Post | Flare Streak | STD | 0..10 | 0 |
| 480 | 481 | 468 | 469 | Floor | Shadow Catcher | STD | 0..1 | 0 |
| 481 | 482 | 469 | 470 | Floor | Catcher Plane | OPT | Floor / Wall | 0 |
| 482 | 483 | 470 | 471 | Floor | Floor Height | STD | -4..2 | -1.02 |
| 483 | 484 | 471 | 472 | Floor | Wall Distance | STD | 0..8 | 1.5 |
| 484 | 485 | 472 | 473 | Floor | Shadow Soft | STD | 0..1 | 0.4 |
| 485 | 486 | 473 | 474 | Floor | Mirror Floor | STD | 0..1 | 0 |
| 486 | 487 | 474 | 475 | Floor | Mirror Blur | STD | 0..1 | 0.2 |
| 487 | 488 | 475 | 476 | Floor | Mirror Fade | STD | 0..1 | 0.2 |
| 488 | 489 | 476 | 477 | Floor | Floor Light | STD | 0..1 | 0.5 |
| 489 | 490 | 477 | 478 | Floor | Floor Camera | STD | 0..1 | 1 |
| 490 | 491 | — | — | FX | Wireframe Mix | STD | 0..1 | 0 |
| 491 | 492 | — | — | FX | Wire Thickness | STD | 0.2..8 | 1 |
| 492 | 493 | — | — | FX | Explode | STD | 0..3 | 0 |
| 493 | 494 | — | — | FX | Explode Random | STD | 0..1 | 0.5 |
| 494 | 495 | — | — | FX | Dissolve | STD | 0..1 | 0 |
| 495 | 496 | — | — | FX | Dissolve Glow | STD | 0..10 | 1 |
| 496 | 497 | — | — | FX | Toon Steps | STD | 0..8 | 0 |
| 497 | 498 | — | — | FX | Outline | STD | 0..1 | 0 |
| 498 | 499 | — | — | FX | Depth Fog | STD | 0..3 | 0 |
| — | — | 478 | 479 | Silhouette | Silhouette | STD | 0..1 | 0 |
| — | — | 479 | 480 | Silhouette | Sil Fill | STD | 0..1 | 0 |
| — | — | 480 | 481 | Silhouette | Sil Glow | STD | 0..10 | 2 |
| — | — | 481 | 482 | Silhouette | Sil Width | STD | 0.5..60 | 8 |
| — | — | 482 | 483 | Silhouette | Sil Color | HUE | 0..1 | 0.523809 |
| — | — | 483 | 484 | Silhouette | Sil Sat | SAT | 0..1 | 0.7 |
| — | — | 484 | 485 | Silhouette | Sil Bri | BRI | 0..1 | 1 |
| — | — | 485 | 486 | Silhouette | Sil Alpha | ALPHA | 0..1 | 1 |
| 499 | 500 | 486 | 487 | Output | Background Color | HUE | 0..1 | 0 |
| 500 | 501 | 487 | 488 | Output | Background Sat | SAT | 0..1 | 0 |
| 501 | 502 | 488 | 489 | Output | Background Bri | BRI | 0..1 | 0 |
| 502 | 503 | 489 | 490 | Output | Background Alpha | ALPHA | 0..1 | 0 |
| 503 | 504 | 490 | 491 | Output | MSAA | OPT | 1x / 4x / 8x | 1 |
| — | 505 | — | 492 | Input | Input Use | OPT | Off / Background / Skin on flagged parts / Skin all | 0 |
| — | 506 | — | 493 | Input | Input Projection | OPT | Front planar / Model UV / Spherical | 0 |
| — | 507 | — | 494 | Input | Input Mix | STD | 0..1 | 1 |
| — | — | — | 495 | Face | Face Track | BOOL | - | 0 |
| — | — | — | 496 | Face | Max Faces | STD | 1..4 | 4 |
| — | — | — | 497 | Face | Face Scale | STD | 0.1..5 | 1.2 |
| — | — | — | 498 | Face | Face Offset X | STD | -1..1 | 0 |
| — | — | — | 499 | Face | Face Offset Y | STD | -1..1 | 0.3 |
| — | — | — | 500 | Face | Follow Roll | STD | 0..1 | 1 |
| — | — | — | 501 | Face | Follow Yaw | STD | 0..1 | 1 |
| — | — | — | 502 | Face | Follow Pitch | STD | 0..1 | 0.6 |
| — | — | — | 503 | Face | Face Smooth | STD | 0..1 | 0.5 |
| — | — | — | 504 | Face | Hide No Face | BOOL | - | 1 |
| 504 | 508 | 491 | 505 | Audio | Audio FFT | BUFFER | 512 bins (FFT) | - |
| 505 | 509 | — | — | Debug | Debug View | OPT | Final / Albedo / Normal / Roughness / Metallic / UV / Part IDs | 0 |
| 506 | 510 | 492 | 506 | Debug | Debug Log | BOOL | - | 0 |

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
