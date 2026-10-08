project:    loadehr
pass:       w2-results
STATUS:     COMPLETE
parent:     8398d50
result:     c0c2f5ef999f5d6c4bcb81dd2f638207dd5e6f1b
session:    43fedbe2-1c90-4fdf-a41a-9e5b7cccfcc3 (LoadEhr Claude Code)
written_at: 2026-10-08T07:48:07Z

# LoadEhr W2 results — mirror room out, Clipper2 + earcut, solid text / logo pieces, Deform group, projector video files

## Amrit's answers (this brief + this session)
- W1 open questions: "1: swap. 2: keep. 3: Keep." -> 1. libtess2 SWAPPED for Clipper2 (Boost) + earcut (ISC); 2. all four model
  slots (A-D) KEPT; 3. the Parts group's relabelling after the dominant model at each hop KEPT.
- "Remove mirror room." (done, HARD RULE 4 + guard G6). "Can projector use video also?" (yes: Stage 5). "Lets add a deform group
  so an object (and text and logo) can be deformed by whatever way you see fit." (Stage 4). "Can text and logo be properly
  shattered?!?" (Stage 3).
- This session: "Arena restarted" (PID 31243 -> 97483).
- Answer to "Can projector use video also?": in Wow FX the projector could already throw the layer's own video (Proj Source =
  Input); W2 adds video FILES to both Wow bundles (Proj File).

## What went
- The Mirror Room: its 20 params, src/Mirror.cpp, MirrorShaders.h, every MIRROR program variant (main / soup / bounce / proj /
  crack / the five marches / the cookie march / bolts / swarm / motes), the LED pass, the mirror prewarm, the LED chase clock + its
  wraps row, tools/w27_mirror.py, the two w26 Stage 10 recipes, `Instance::bounces`. Guard G6.
- libtess2 (SGI Free Software License B): third_party/libtess2, CMake, VERSIONS.txt. Guard G7.

## What was added
- Clipper2 2.0.1 + earcut v3.2.4 (licences in third_party/clipper2, third_party/earcut). Clipper2 Union resolves the source fill
  rule into a PolyTree (outers CCW, holes CW, as libtess2 gave), earcut fills each outer with its holes.
- Shatter Mode (Shape, every bundle): Letters / Pieces (default) / Pieces + Letters. Solid pieces for text and logos.
- Deform group (every bundle, 25 dials): Deform Amount, Deform Axis, Taper, Twist, Bend, Squash, Squash Beat, Shear, Bulge,
  Spherize, Noise Warp, Warp Scale, Warp Speed, Ripple, Ripple Freq, Ripple Speed, Ripple Beat, Melt, Drip, Glitch Shift, Glitch
  Bands, Glitch Rate (steps/s, the optional rate dial), Mirror Fold, Jelly, Audio > Deform. Order: Letters -> Taper, Twist, Bend,
  Squash, Shear, Bulge, Spherize, Noise Warp, Ripple, Melt / Drip, Glitch, Mirror Fold, Jelly -> shatter / slice flight.
- Projector: Proj Image -> Proj File (png jpg mov mp4 m4v), Proj Source (Wow FX) Input / File, Video Speed, Video Loop
  (Loop / Ping-Pong / Once), Video Restart.

## The shatter design (Stage 3)
With Shatter Mode Pieces the text / logo spec carries `frac=<Shard Count>`, so the fractured copy is built off the render thread
with the spec and cached by it (a new Shard Count rebuilds it; text ~10-30 ms). Per letter: seeds on its front cap (Shard Count
shared by front area, >= 1 per letter, best-of-12 spacing), 2D Voronoi cells; every finished triangle of the letter (caps, bevel,
side wall) is clipped by each convex cell (Sutherland-Hodgman in barycentric space, all attributes interpolated) — so the outer
faces of the pieces ARE the letter's surface (same placement, same normals: no pop) — and each cell edge through the solid gets a
flat cut face between the top and bottom section lines (part "Core", the slot after Bevel, with Visible / Hue Shift / Glow /
Surface). The copy rides in the same buffers as extra parts; the whole mesh draws at rest and once reformed (bit-identical,
max 0), the copy only while pieces fly. Pieces + Letters gives a letter's pieces its letter's seed (one kick and spin per letter).
Bounce, the Shatter transition, Lava Split and the Letters dials work unchanged on the pieces.
DEVIATION (why): the brief said "cell intersected with the shape by Clipper2, each piece extruded"; re-extruding cell∩shape cannot
keep the bevel on the original outline only, nor match the whole mesh exactly at home. Clipping the finished letter by convex cells
gives both. Clipper2 does the outlines (Stage 2), not the pieces.
Loaded 3D models (one line, as asked): solid pieces would need the same cell clipping in 3D plus capping each cut with the
mesh's cross-section polygon — which only works on watertight meshes, and most show models (glTF scans, game assets) are not.

## Validated (numbers)
- Regress (6 references) max 0 after every stage and on the final build; guards G1-G7 pass on all four bundles.
- Stage 1 parity vs W1 (64 looks, 552 shots; W1 reference run 4x for its own spread): identical 24, float noise 6, within W1's
  run-to-run 34, DIFFERS 0. (With 2 reference runs 14 looked like regressions; all were chaotic smoke / realtime looks.)
- Final-build parity vs W1: identical 17, float noise 6, within spread 33, 8 explained differences: Spots now carried by LoadEhr
  (W1's classic scripts set Spot dials that did nothing there: w4_post, w11_colours), Twist / Bend / Melt / Ripple now shade with
  deformed normals (w6_shape: same shapes, smooth instead of faceted — Stage 4 item 4), realtime / chaotic looks just over W1's
  spread (w6 slices / contours / dust, w7 echoes, w9 face clip, Smoulder, Lava Split, Projection Mapped).
- Stage 2 (tools/w2_text.py, docs/parity/w2_text, 15 strips): silhouettes 0 differing pixels in all 15 cases (W1 vs W2, max 0);
  triangle counts identical except CJK 20958 -> 20840; lit 3/4 views bit-identical except my self-cross SVG, whose two shapes
  overlap in one plane (z-fighting, triangulation-dependent).
- Stage 3 (tools/w2_shatter.py, docs/parity/w2_shatter, 13 strips): LOADEHR 8 / 64 / 256 -> 7 / 65 / 256 pieces (7.2k / 17.5k /
  46k extra tris), two rows, SVG + PDF logos, Letters / Pieces / Pieces + Letters, Bounce (text in Wow, SVG in LoadEhr), the
  Shatter transition text -> model (back at 0 = first 0, max 0): every reformed frame = rest frame, max 0 (the waving-letters case
  differs by design).
- Stage 4 (tools/w2_deform.py): every deform rendered on the skull + two on text; cost (harness 1080p, 31k tris, GPU ms vs every
  deform 0): -0.2..+0.5 each (noise level), Mirror Fold X+Y+Z +1.6, everything at Amount 2 +1.3. 21 DEFORM twins, no new samplers.
  Clocks: Noise Warp / Glitch wraps ok, deform scene flat over 180 min (max change 10.9 at 180 vs 10.7 at 1 min).
- Stage 5 (tools/w2_video.py, 13 files): every codec plays (frame 1 s != 3 s). Decode ms / frame on the worker (harness): H.264
  1.96 (1080p) / 6.24 (4K), HEVC 1.30 / 3.05, ProRes 422 0.73 / 1.30, ProRes 4444 alpha 0.79, HAP 2.68 / 7.54, HAP Alpha 4.53,
  HAP Q 3.56 / 9.07, HAP Q Alpha 4.66; render-thread upload 0.17-1.04 ms (4K with Focus mips 1.58); decode cache 63 / 253 MB (BGRA
  1080p / 4K), 8-63 MB (HAP); late frames 0-1 for HAP, 6-21 for the AVFoundation codecs (all in the first second, before the window
  fills). Play modes exact: Loop t = t + 6 s = t + 12 s (max 0), Ping-Pong back at the start after 12 s (max 0), Once holds.
  ARENA (MBP M3 Max, Wire closed, clip selected for preview, not triggered): H.264 1080p clip 80-89 fps delivered, plugin CPU
  1.3-1.6 ms, projector pass 0.22-0.26 ms, upload 0.16-0.35 ms, 0 late; HAP 1080p 87-90 fps, CPU 1.26-1.35 ms, 0 late.
- Bundles: LoadEhr 482, FX 486, Wow 487, Wow FX 501 (all <= 512; universal arm64 + x86_64).

## Bundle table (groups per bundle; FFGL index order)
| group | LoadEhr | LoadEhr FX | Wow | Wow FX |
|---|---|---|---|---|
| Model (+ chain), Text, Transform, Camera, Motion, Material, Parts, Lighting, Volume, Smoke Sim, Light A / B / C, Post, Floor, Output, Audio, Debug Log | yes | yes | yes | yes |
| Shape: Shatter / Reform / On Beat / Shard Count / Force / Gravity / Spin / Reform Time, Shatter Mode, Shatter Physics, Bounciness, Floor Friction | yes | yes | yes | yes |
| Deform (W2, 25) | yes | yes | yes | yes |
| Logo, Extrude, Letters, Camera Live, Surface (all but Glass), Shape (Melt-free rest: slices, voxel, low poly, tumble, dust, contours), Copies, FX, Debug View | yes | yes | — | — |
| Spots (W2: moved here) | yes | yes | — | — |
| Project Input | — | yes | — | — |
| Input | — | yes | — | yes |
| Trip (W2: moved here) | — | — | yes | yes |
| Liquid (Model), Physics Speed, Fire, Lightning, Swarm, Lava, Caustics, Projector (+ video), Silhouette | — | — | yes | yes |
| Face | — | — | — | yes |
What moved and why: Deform had to be in all four; then 2 x shared + both sides' extras = 1044 > 2 x 512 for the two FX carriers,
so no amount of moving between classic-only and Wow-only groups could fit: one SHARED group had to become single-carrier. Trip (60,
smoke looks, the Wow kind of thing) went to Wow; that left Wow FX 4 over, so Spots (15, not used by any Wow look) moved to classic;
Shatter Physics / Bounciness / Floor Friction became shared so a logo (classic-only) can bounce (Physics Speed stays Wow; classic
flies at 1). Nothing was cut. Names <= 16 chars, no colour without an HSBA quartet, no range shrunk.

## Deviations (and why)
- The fracture clips the finished letter by convex cells instead of Clipper2 cell∩shape + re-extrusion (see the design).
- Bulge rounds the middle of the axis outward (radial), not along vertex normals: per-vertex normals split at hard edges and tear
  text (cap vs side) open. Negative = pinch, as asked.
- Twist / Bend / Ripple / Melt keep their meaning but now draw through the DEFORM twin with deformed normals (smooth instead of
  W1's flat facets) — the only W1 look change outside the removals (w6_shape strips).
- Dust points (Blow To Dust) are not deformed (they start from the rigid surface), like the SDFs and the eye finder.
- DXV not supported: the only reachable decoder (Glance / glenc-public) ports FFmpeg's LGPL dxv.c. HAP R / HDR (BC7 / BC6)
  refused (no BPTC on this GL). HAP test media made with Amrit's glenc-cli (ffmpeg here has no HAP encoder).
- The installed bundles' plugin description still says "mirror room" (fixed in source after the install, c0c2f5e); it updates
  with the next install. No behaviour difference.
- Stage-1 parity first ran L and a W1 run at the same time: realtime-dependent looks diverged; re-run alone.

## Arena (comp "LoadEhr", not saved; clips untriggered; selection parked on layer 4 column 17, empty)
Layer 4 "LoadEhr W2": C1 Text Pieces Core (Wow: LOADEHR gold, Pieces 64, Shatter On Beat Every 4, Core Glow 1.2) | C2 Text
Letters (the same, Letters: the comparison) | C3 Logo Pieces Bounce (LoadEhr: test_emblem.svg, Pieces 64, Bounce, On Beat Every 8,
Shadow Catcher) | C4 Deform Taper | C5 Deform Squash Beat | C6 Deform Shear | C7 Deform Bulge | C8 Deform Spherize | C9 Deform Noise
Warp | C10 Deform Jelly (wobble + beat bounce drive it) | C11 Deform Glitch | C12 Deform Mirror Fold (X) | C13 Deform Everything x2
(Deform Amount 2) | C14 Deform Text Twist (LOADEHR, Twist X + Noise Warp) | C15 Projector Video H264 (Wow: faces_clip.mp4 on the skull
in haze) | C16 Projector Video HAP (the HAP version). C4-C13 on the Human Skull by path, Spin Y 4. W1 clips untouched (L2 C2
"Mirror Fire" was a mirror-room clip: it now renders without the room).

## Open questions / for Amrit
- Prune after playing: four slots, the 25 Deform dials, Glitch Rate, Shatter Mode default Pieces.
- Trip is now Wow-only and Spots classic-only (the 512 cap): OK, or would you rather another group move?

## Commits
- c0c2f5e LoadEhr W2 (pushed, main...origin/main). This handoff: the commit after it.
- Commons: LOADEHR-001..004 (licence rulings incl. DXV LGPL; AVAssetReader HAP marker buffers; preprocessor-only GLSL changes stay
  bit-identical; projector video cost in Arena), synced (ffgl-commons 709467d).

## RECEIPTS
```
$ git log -1 --stat | head -3 ; git status -sb
commit c0c2f5ef999f5d6c4bcb81dd2f638207dd5e6f1b
Author: Amrit Rosell <amrit.rosell@gmail.com>
Date:   Thu Oct 8 09:44:10 2026 +0200
## main...origin/main

$ tools/regress.sh w2/regress_final (compares + guards)
compare verify/w1/ref/r1_mask_front.png vs verify/w2/regress_final/r1_mask_front.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r2_wireframe50.png vs verify/w2/regress_final/r2_wireframe50.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r3_explode.png vs verify/w2/regress_final/r3_explode.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r4_fx_skin_env_input.png vs verify/w2/regress_final/r4_fx_skin_env_input.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r5_chain_mid_hop_b_c.png vs verify/w2/regress_final/r5_chain_mid_hop_b_c.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r6_chain_rest_d.png vs verify/w2/regress_final/r6_chain_rest_d.png: mean 0.0000 max 0 pixels>2 0 of 2073600
G1 ok   LoadEhr: 482 params, none named Library* / Laser*
G6 ok   LoadEhr: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhr: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model
G1 ok   LoadEhrFX: 486 params, none named Library* / Laser*
G6 ok   LoadEhrFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a mod
G1 ok   LoadEhrWow: 487 params, none named Library* / Laser*
G6 ok   LoadEhrWow: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWow: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a mo
G1 ok   LoadEhrWowFX: 501 params, none named Library* / Laser*
G6 ok   LoadEhrWowFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWowFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a 
G2 ok   the only directory API: ModelLoader.cpp FolderIndex (textures next to the chosen model file):
G3 ok   no model / texture file inside build/LoadEhr*.bundle (8 files: binaries + Info.plist)
G5 ok   no Laser* / Library* sources, no library tools
G6 ok   no Mirror* source, no MIRROR program variant
G7 ok   no libtess2 / tesselator / SGI-B in src, third_party, CMakeLists.txt

$ lipo -info build/LoadEhr*.bundle/Contents/MacOS/*
Architectures in the fat file: build/LoadEhr.bundle/Contents/MacOS/LoadEhr are: x86_64 arm64 
Architectures in the fat file: build/LoadEhrFX.bundle/Contents/MacOS/LoadEhrFX are: x86_64 arm64 
Architectures in the fat file: build/LoadEhrWow.bundle/Contents/MacOS/LoadEhrWow are: x86_64 arm64 
Architectures in the fat file: build/LoadEhrWowFX.bundle/Contents/MacOS/LoadEhrWowFX are: x86_64 arm64 

$ grep receipts (src, tools, CMakeLists.txt, third_party)
MIRROR program variants / Mirror sources: 1 files; ls src/Mirror*: 0
libtess / tesselator / SGI-B: 0 files; third_party/libtess2: 0
Mirror Room params in ParamList.inc: 0

$ ls -la ~/Documents/Resolume Arena/Extra Effects | grep LoadEhr
drwxr-xr-x   3 amritrosell  staff     96 Oct  7 20:05 LoadEhr.bundle
drwxr-xr-x   3 amritrosell  staff     96 Oct  7 20:05 LoadEhrFX.bundle
drwxr-xr-x   3 amritrosell  staff     96 Oct  7 20:05 LoadEhrWow.bundle
drwxr-xr-x   3 amritrosell  staff     96 Oct  7 20:05 LoadEhrWowFX.bundle

$ du -sh docs/parity/w2_text docs/parity/w2_shatter; ls docs/parity/w2_text | wc -l; ls docs/parity/w2_shatter | wc -l
1.1M	docs/parity/w2_text
1.3M	docs/parity/w2_shatter
      15
      13

$ ls -la test_media/gen (not committed)
    
480 Oct 8 09:20 .
256 Oct 8 09:16 ..
3191851 Sep 30 11:59 h264_1080.mp4
9328531 Oct 8 09:16 h264_4k.mp4
162579299 Oct 8 09:17 hap_1080.mov
580587087 Oct 8 09:18 hap_4k.mov
197612366 Oct 8 09:19 hapalpha_1080.mov
300782273 Oct 8 09:19 hapq_1080.mov
1119965059 Oct 8 09:22 hapq_4k.mov
334444928 Oct 8 09:20 hapqalpha_1080.mov
1444038 Oct 8 09:16 hevc_1080.mp4
3702410 Oct 8 09:16 hevc_4k.mp4
116165966 Oct 8 09:16 prores422_1080.mov
422707031 Oct 8 09:16 prores422_4k.mov
376333857 Oct 8 09:16 prores4444a_1080.mov

$ tools/parity.py report verify/w2/parity1b (stage 1: mirror room out, vs W1 x4 runs)
looks: 64, shots: 552 (each look at its worst shot)
  identical                            24
  float noise (max <= 2)               6
  noise (within MaskEhr run-to-run)    34

$ tools/w2_text.py run verify/w2/text <W1 build> (table.md)
| case | W1 tris | W2 tris | shapes W1 / W2 | covered px W1 | W2 | silhouette px differing (>2/255) | share of covered | lit 3/4 view: mean / max / px>2 
|---|---|---|---|---|---|---|---|---
| holes | 19206 | 19206 | 6 / 6 | 14726 | 14726 | 0 | 0.000 % | 0.0000 / 0 / 0 
| bold | 18274 | 18274 | 6 / 6 | 17821 | 17821 | 0 | 0.000 % | 0.0000 / 0 / 0 
| italic | 17748 | 17748 | 6 / 6 | 14826 | 14826 | 0 | 0.000 % | 0.0000 / 0 / 0 
| spacing | 9016 | 9016 | 4 / 4 | 14157 | 14157 | 0 | 0.000 % | 0.0000 / 0 / 0 
| rows | 9658 | 9658 | 8 / 8 | 43493 | 43493 | 0 | 0.000 % | 0.0000 / 18 / 1 
| cjk | 20958 | 20840 | 4 / 4 | 22000 | 22000 | 0 | 0.000 % | 0.0001 / 18 / 5 
| arial_file | 15278 | 15278 | 5 / 5 | 19988 | 19988 | 0 | 0.000 % | 0.0000 / 5 / 1 
| loadehr | 7218 | 7218 | 7 / 7 | 12989 | 12989 | 0 | 0.000 % | 0.0000 / 0 / 0 
| svg_nested | 256 | 256 | 1 / 1 | 157276 | 157276 | 0 | 0.000 % | 0.0000 / 0 / 0 
| svg_selfcross | 1544 | 1544 | 2 / 2 | 105340 | 105340 | 0 | 0.000 % | 0.5544 / 124 / 7605 
| svg_star_evenodd | 220 | 220 | 1 / 1 | 49883 | 49883 | 0 | 0.000 % | 0.0000 / 0 / 0 
| svg_star_nonzero | 156 | 156 | 1 / 1 | 65325 | 65325 | 0 | 0.000 % | 0.0000 / 0 / 0 
| svg_emblem | 4448 | 4448 | 2 / 2 | 104764 | 104764 | 0 | 0.000 % | 0.0000 / 0 / 0 
| pdf_emblem | 4448 | 4448 | 2 / 2 | 104764 | 104764 | 0 | 0.000 % | 0.0000 / 0 / 0 
| png_traced | 11590 | 11590 | 2 / 2 | 104410 | 104410 | 0 | 0.000 % | 0.0000 / 0 / 0 

$ tools/w2_shatter.py run verify/w2/shatter (report.txt)
loadehr_pieces_64        7218 tris, 7 letters/shapes + 65 solid pieces (17510 tris) | reformed vs rest: mean 0.0000 max 0 pixels>2 0 of 518400
loadehr_pieces_8         7218 tris, 7 letters/shapes + 7 solid pieces (7218 tris) | reformed vs rest: mean 0.0000 max 0 pixels>2 0 of 518400
loadehr_pieces_256       7218 tris, 7 letters/shapes + 256 solid pieces (46042 tris) | reformed vs rest: mean 0.0000 max 0 pixels>2 0 of 518400
loadehr_letters          7218 tris, 7 letters/shapes | reformed vs rest: mean 0.0000 max 0 pixels>2 0 of 518400
loadehr_pieces_letters   7218 tris, 7 letters/shapes + 65 solid pieces (17510 tris) | reformed vs rest: mean 0.0000 max 0 pixels>2 0 of 518400
two_rows                 7218 tris, 7 letters/shapes + 65 solid pieces (17510 tris) | reformed vs rest: mean 0.0000 max 0 pixels>2 0 of 518400
svg_logo                 4448 tris, 2 letters/shapes + 64 solid pieces (17564 tris) | reformed vs rest: mean 0.0000 max 0 pixels>2 0 of 518400
pdf_logo                 4448 tris, 2 letters/shapes + 64 solid pieces (17564 tris) | reformed vs rest: mean 0.0000 max 0 pixels>2 0 of 518400
svg_bounce               (LoadEhr) logo pieces + Bounce | reformed vs rest: mean 0.0000 max 0 pixels>2 0 of 518400
text_bounce              7218 tris, 7 letters/shapes + 65 solid pieces (17510 tris) | reformed vs rest: mean 0.0000 max 0 pixels>2 0 of 518400
text_letters_wave        7218 tris, 7 letters/shapes + 65 solid pieces (17510 tris) | reformed vs rest: mean 3.9875 max 204 pixels>2 19192 of 518400
core_glow                5060 tris, 2 letters/shapes + 24 solid pieces (10128 tris) | reformed vs rest: mean 0.0000 max 0 pixels>2 0 of 518400
trans_text_to_model      transition 0 -> 1 -> 0: back at 0 vs first 0: mean 0.0000 max 0 pixels>2 0 of 518400

$ tools/w2_deform.py run verify/w2/deform (cost.md, glitch / everything rerun after the normal fix)
| case | wall ms avg | p99 | GPU ms avg | p99 | GPU ms vs every deform 0 |
|---|---|---|---|---|---|
| glitch | 4.74 | 10.87 | 2.13 | 7.88 | - |
| everything_x2 | 6.36 | 13.53 | 3.11 | 10.53 | - |

$ tools/w2_video.py run verify/w2/video (video.md)
 file | decode ms / frame (worker) | upload ms (render thread, last / worst of the 1 s lines) | late frames (of ~180) | decode cache MB | plugin wall ms avg / p99 | GPU ms avg 
---|---|---|---|---|---|---
 h264_1080.mp4 | 1.96 | 0.33 / 0.33 | 8 | 63 | 9.38 / 15.12 | 6.12 
 h264_4k.mp4 | 6.24 | 0.79 / 0.79 | 21 | 253 | 9.02 / 14.61 | 6.10 
 hevc_1080.mp4 | 1.30 | 0.20 / 0.20 | 6 | 63 | 8.47 / 13.66 | 5.48 
 hevc_4k.mp4 | 3.05 | 0.17 / 0.18 | 12 | 253 | 9.08 / 14.34 | 5.95 
 prores422_1080.mov | 0.73 | 0.33 / 0.33 | 6 | 63 | 9.26 / 14.91 | 6.07 
 prores422_4k.mov | 1.30 | 0.57 / 0.57 | 8 | 253 | 9.45 / 15.53 | 5.97 
 prores4444a_1080.mov | 0.79 | 0.37 / 0.37 | 6 | 63 | 9.08 / 14.90 | 5.61 
 hap_1080.mov | 2.68 | 0.54 / 0.54 | 0 | 8 | 8.74 / 13.97 | 5.65 
 hap_4k.mov | 7.54 | 0.54 / 0.55 | 1 | 32 | 8.67 / 15.09 | 5.23 
 hapalpha_1080.mov | 4.53 | 0.74 / 0.74 | 0 | 16 | 8.66 / 14.99 | 5.14 
 hapq_1080.mov | 3.56 | 0.77 / 0.77 | 0 | 16 | 9.39 / 14.63 | 5.96 
 hapq_4k.mov | 9.07 | 0.90 / 0.91 | 1 | 63 | 7.97 / 13.60 | 5.24 
 hapqalpha_1080.mov | 4.66 | 1.04 / 1.04 | 0 | 24 | 9.30 / 14.89 | 6.08 

$ tools/w14_wraps.py after (new rows) / times deform scene
Ripple 20: ratio 0.89 ok | Noise Warp drift 4 cells/s, wrap 64 s: ratio 0.76 ok | Glitch step 30/s, wrap 33.3 s: ratio 0.62 ok
deform scene max per-frame change: 1 min 10.676 | 13: 11.555 | 26: 13.035 | 60: 11.613 | 180: 10.889 (median 6.6-7.2)

$ Arena REST (comp "LoadEhr", PID 31243 -> 97483), layer 4 "LoadEhr W2", selection parked on L4 C17 (empty)
comp LoadEhr
layer 4 LoadEhr W2
  C1 Text Pieces Core           Disconnected
  C2 Text Letters               Disconnected
  C3 Logo Pieces Bounce         Disconnected
  C4 Deform Taper               Disconnected
  C5 Deform Squash Beat         Disconnected
  C6 Deform Shear               Disconnected
  C7 Deform Bulge               Disconnected
  C8 Deform Spherize            Disconnected
  C9 Deform Noise Warp          Disconnected
  C10 Deform Jelly               Disconnected
  C11 Deform Glitch              Disconnected
  C12 Deform Mirror Fold         Disconnected
  C13 Deform Everything x2       Disconnected
  C14 Deform Text Twist          Disconnected
  C15 Projector Video H264       Disconnected
  C16 Projector Video HAP        Disconnected
Arena PID: 97483 /Applications/Resolume Arena/Arena.app/Contents/MacOS/Arena
```
END OF HANDOFF w2-results cb49e9f1 22095
