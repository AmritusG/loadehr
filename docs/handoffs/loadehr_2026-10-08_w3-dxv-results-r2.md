project:    LoadEhr
pass:       w3-dxv
STATUS:     COMPLETE
parent:     7f4bf14
result:     2469c66 f30ee5a
session:    44ca7128-e1da-4ce2-8f0a-77781723428a
written_at: 2026-10-08T15:06:00+02:00
# LoadEhr W3 results: the projector plays DXV through DXVKit, + Video Alpha

## What shipped (commits 2469c66 + f30ee5a, pushed; supersedes the BLOCKED handoff and loadehr_2026-10-08_w3-dxv-results.md, whose receipts showed G7 FAILing on the BLOCKED handoff quoting the planted LGPL tag: fixed in f30ee5a, docs/handoffs left out of the LGPL scan)
- DXVKit is a git submodule at third_party/dxvkit, pinned at cdbb4bc (MIT), built by our CMake as a static universal C lib (no tools,
  no Swift). Fresh checkouts need `git submodule update --init` (in CLAUDE.md).
- Video.mm RunDxv: every Proj File video is opened with dxvk_mov_open_file first. A DXV track plays through DXVKit; anything else
  falls back to AVFoundation. The same worker, decode window (Window(), now shared), late-frame rule, Loop / Ping-Pong / Once / Speed /
  Restart / clk.video. Each packet is a whole frame, so the wanted frame is read and decoded directly.
- Upload: DXT1 -> opaque BC1 (0x83F0), DXT5 -> 0x83F3, YCG6 -> RGTC1 Y + RGTC2 CoCg (half size), YG10 -> RGTC2 Y+A + RGTC2 CoCg. The
  existing uDxt / uDxtA samplers are reused (no new sampler). uDxtFit crops the 16-px padding. YCoCg uses Hap Q's formula with scale 1.
- DEVIATION: HQ chroma is sampled NEAREST, not linear. Linear smeared colour edges (testsrc2: max 107/255 vs DXVKit) and pulled the
  padding's chroma into the bottom row (97). Nearest is the DXV spec's convention and has no edge bleed.
- Video Alpha (Projector, Wow + Wow FX): Premultiplied (default) / Straight, for every codec with alpha. Counts: LoadEhr 482, FX 486,
  Wow 488, Wow FX 502.
- G7 now fails on any LGPL file in the tree, submodules included (tested: a planted LGPL SPDX tag FAILs).
- Debug only: LOADEHR_VIDEO_DUMP=<folder/> writes each new picture as raw RGBA (used for the proof).
- CLAUDE.md: the "DXV: not supported" line is gone; DXV + VIDEO ALPHA sections added; param map regenerated.
- Commons: LOADEHR-005 (DXV in an FFGL plugin via DXVKit + Arena cost), LOADEHR-006 (a file does not say straight vs premultiplied).

## Validated (evidence in verify/w3/, gitignored like all of verify/: local only)
- PROOF (tools/w3_dxv.py proof; the picture texture vs `dxvtool dump-rgba` of the same frames, 3 frames per file, crop included):
  all 8 Resolume vectors + Metric_01 (legacy DXT1) + GREAT SURRENDER (DXT1) + black_snowflake (DXT5 4K, picture 1/2) + AWAKE (YCG6) +
  YG10-DXV3 + S06 (YCG6 908x2276) + S08 (DXT5 908x2276). Max |d| per variant: DXT1 0-2, legacy 2, DXT5 1-2, YCG6 1-3, YG10 2-4 (of 255);
  mean 0.00-0.53. Last row / column max <= 2: no padding stripe. CAUSE of the residual: the GPU interpolates the BCn palettes at
  higher precision than DXVKit's integer (truncating) palette. A float-exact CPU decode of the same DXT1 / YCG6 blocks matches the
  picture on 98.6 % of values, the rest +-1 (vs 84 % / 72 % against DXVKit's RGBA). The planes are DXVKit's bytes, uploaded unchanged.
- ALPHA ("are all versions of DXT done properly, alpha and all?"): Straight picture vs `dxvtool --premultiply`: DXT5 max 3 (mean 0.04),
  YG10 max 3 (mean 0.08). Opaque pixels (a = 255) are identical Straight vs Premultiplied (max 0): Straight darkens nothing opaque.
  Strips: verify/w3/alpha/alpha_DXT5_DXV3.png, alpha_YG10_DXV3.png. These files have black under alpha 0, so only soft edges move.
- NOTE: black_snowflake "DXV Normal Quality With Alpha" is DXT5 but alpha 255 in every frame checked (1, 60, 150, 300, 440). So the
  alpha demos use Testfiles/DXT5-DXV3.mov (38 % transparent); snowflake stays as the 4K DXT5 case.
- W2 FILES (frame 0, vs the W2 build): H.264, HEVC, ProRes 422, HAP 1080 / 4K, Hap Q: max 0 in both modes. ProRes 4444 / HAP Alpha /
  HAP Q Alpha: Straight = W2 max 0. Premultiplied (the NEW DEFAULT) mean 2.55, max 148, ON PURPOSE. Both ways:
  verify/w3/w2files/strip_prores4444a.png. Our ffmpeg-made 4444 file keeps colour under alpha 0, so read premultiplied, the whole
  frame lights the skull. Clips with a straight-alpha file need Video Alpha Straight (as in Arena's clip Alpha setting).
- LONG SHOW (Loop, S08 5.04 s clip, harness advance): frame shown at 0.5 / 60 / 120 / 180 min = 115 / 6 / 12 / 18 = exact.
- COST, harness 1080p, real time, 4 s (cost.md): 0 late frames on all 15 files. Worker decode 0.19-0.73 ms (vectors), 1.0-6.5 ms
  (real files incl. I/O). Upload 0.66-1.6 ms at 1080p, 1.9-2.4 ms for 4K DXT5.
- COST, Arena 7.28, MBP M3 Max, Wire closed, 120 Hz panel, each clip SELECTED (not triggered), Debug Log PERF / VIDEO, Arena PID
  77631: ~120 fps delivered for every variant (the panel cap: a floor), plugin CPU 0.33-0.38 ms. Decode / upload ms: DXT1 0.52 / 0.16,
  legacy 2.04 / 0.15, DXT5 1.05 / 0.18, YCG6 1.69 / 0.19, YG10 2.34 / 0.21, portrait YCG6 1.18 / 0.20, DXT5 4K 2.20 / 0.85. 0 late.
- Jobs callback not used: YG10 1080p block decode 1.74 -> 1.46 ms with 2 threads, a 0.3 ms saving on a 33 ms worker budget.
- GL (M3 Max, "4.1 Metal - 90.5"): S3TC present; RGTC1 / RGTC2 upload with no error although GL_COMPRESSED_TEXTURE_FORMATS lists only
  the S3TC three; BPTC INVALID_ENUM.

## Arena clips (layer 5 "LoadEhr W3", opacity 1, Wow / Wow FX, skull in haze, soft; UNTRIGGERED; selection parked on L5 C17)
C1 DXT1 (GREAT SURRENDER) · C2 DXT5 alpha (DXT5-DXV3) · C3 YCG6 (AWAKE) · C4 YG10 (YG10-DXV3) · C5 legacy (Metric_01) ·
C6 DXT5 4K (black_snowflake) · C7 portrait YCG6 908x2276 (S06) ·
C8 COMPARE (a): DXT5-DXV3 as the clip source + LoadEhr Wow FX, Proj Source File = the same file (DXVKit) ·
C9 COMPARE (b): same source + Wow FX, Proj Source Input (Arena's native DXV decode). Same dials, Input Use Off ·
C10 DXT5-DXV3 Video Alpha Straight · C11 the same, Premultiplied. Dials: tools/arena_w3.py 5. W1 / W2 clips not touched.

## Amrit's answers this session
- Granted the permission (/permissions: allow `git submodule add https://github.com/AmritusG/dxvkit.git third_party/dxvkit`), then "done".
- Restarted Arena when asked (PID 97483 -> 77631).

## Open / for Amrit's eyes
- C8 vs C9: does DXVKit's picture match Arena's own DXV playback (colour, alpha)? This is the visual check the harness cannot do.
- The default flip to Premultiplied changes W2 clips that use HAP Alpha / HAP Q Alpha / ProRes 4444 files with straight colour. Set
  Video Alpha Straight on those to get the W2 look back.
- The commit message names verify/w3, but verify/ is gitignored: that evidence is local only (as in earlier waves).

## RECEIPTS
```
$ git log -2; git status -sb; git submodule status
f30ee5a316c37e6da50f06174b7af67f04bfb3ec G7: docs/handoffs are prose (the W3 BLOCKED handoff quotes the planted LGPL tag): left out of the LGPL scan
2469c66fd17404f00755beab430b38d08387cde0 LoadEhr W3: the projector plays DXV files through DXVKit; Video Alpha switch
## main...origin/main
 cdbb4bc0f2407d7c3c88d4b050592e0e1fec2da7 third_party/dxvkit (cdbb4bc)
$ tools/regress.sh (reference set + guards)
ref-hash guard ok: 6 references match tools/regress_refs.sha256
compare verify/w1/ref/r1_mask_front.png : mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r2_wireframe50.png : mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r3_explode.png : mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r4_fx_skin_env_input.png : mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r5_chain_mid_hop_b_c.png : mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r6_chain_rest_d.png : mean 0.0000 max 0 pixels>2 0 of 2073600
G1 ok   LoadEhr: 482 params, none named Library* / Laser*
G6 ok   LoadEhr: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhr: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G1 ok   LoadEhrFX: 486 params, none named Library* / Laser*
G6 ok   LoadEhrFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G1 ok   LoadEhrWow: 488 params, none named Library* / Laser*
G6 ok   LoadEhrWow: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWow: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G1 ok   LoadEhrWowFX: 502 params, none named Library* / Laser*
G6 ok   LoadEhrWowFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWowFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G2 ok   the only directory API: ModelLoader.cpp FolderIndex (textures next to the chosen model file):
G3 ok   no model / texture file inside build/LoadEhr*.bundle (8 files: binaries + Info.plist)
G5 ok   no Laser* / Library* sources, no library tools
G6 ok   no Mirror* source, no MIRROR program variant
G7 ok   no libtess2 / tesselator / SGI-B in src, third_party, CMakeLists.txt
G7 ok   no LGPL file in the tree (609 tracked files, submodules included)
$ ls -la verify/w3/*
-rw-r--r--  1 <user>  staff     456 Oct  8 14:39 alpha.md
-rw-r--r--  1 <user>  staff  467621 Oct  8 14:39 alpha_DXT5_DXV3.png
-rw-r--r--  1 <user>  staff  467361 Oct  8 14:39 alpha_YG10_DXV3.png
-rw-r--r--  1 <user>  staff  9892 Oct  8 15:03 perf_lines.txt
-rw-r--r--  1 <user>  staff  1916 Oct  8 14:41 cost.md
-rw-r--r--  1 <user>  staff  274 Oct  8 14:38 longshow.md
-rw-r--r--  1 <user>  staff  5218 Oct  8 14:37 proof.md
-rw-r--r--  1 <user>  staff  248793 Oct  8 14:26 strip_prores4444a.png
-rw-r--r--  1 <user>  staff    1762 Oct  8 14:24 w2_files_vs_w2.py
$ verify/w3/longshow/longshow.md
| 0.5 min | 115 | 115 |
| 60 min | 6 | 6 |
| 120 min | 12 | 12 |
| 180 min | 18 | 18 |
$ Arena layer 5 (REST)
LoadEhr W3 opacity 1.0
 C1 W3 DXV DXT1
 C2 W3 DXV DXT5 Alpha
 C3 W3 DXV YCG6
 C4 W3 DXV YG10
 C5 W3 DXV Legacy
 C6 W3 DXV DXT5 4K
 C7 W3 DXV Portrait
 C8 W3 Compare DXVKit
 C9 W3 Compare Arena Native
 C10 W3 Alpha Straight
 C11 W3 Alpha Premultiplied
```
END OF HANDOFF w3-dxv sha256:fe59ba71 10076
