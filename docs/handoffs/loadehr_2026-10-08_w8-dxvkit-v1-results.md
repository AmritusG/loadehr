project:    LoadEhr
pass:       w8-dxvkit-v1-results
STATUS:     COMPLETE
parent:     dc2ba97
result:     dcebbb8 (code; this handoff committed on top)
session:    8e8545d5-3aaa-4cb3-baaf-d74ec86ff73c
written_at: 2026-10-08T21:22:44Z

# LoadEhr W8 results: DXVKit v1.0.0 in the projector (Arena-exact DXV acceptance, HQ colour midpoint 127.5)

Brief: `docs/handoffs/loadehr_2026-10-08_w8-dxvkit-v1-brief.md`. Code commit dcebbb8 (pushed). This handoff is committed on top of it.

## Amrit's words
- On W7: "Now all transtitions looks amazing!" So the W7 open questions are closed as accepted. The Twist / Melt top ends stay as
  built, and the sparkle level the clips use stays (no default change).
- On DXVKit (earlier): "Clean room." / "Keep exact."
- This session: restarted Arena when asked ("done"; PID 53933 -> 27709).

## What shipped
- **Bump.** `third_party/dxvkit` now points at tag v1.0.0 (2f5256a). VERSIONS.txt updated. G7 (no LGPL file, submodules included) still ok.
- **Video.mm.**
  - Every DXVKit call passes DXVK_GEN_AUTO, so the packet decides legacy vs DXV3.
  - Frame 0 is now fully decoded when the file opens. If it fails, the file is REFUSED: Video Time shows "video failed: DXV frame 0
    of <file>: <status>" and the log gets one line. This covers DXVK_ERR_BAD_HEADER (DXV3 version byte < 4, stored flag > 1), which
    Arena plays black.
  - A later frame that fails no longer kills the whole video (W3 stopped the worker). The frame is marked bad and never fetched again,
    the last frame stays, and with Debug Log there is one VIDEO line per file.
  - There was no UNSUPPORTED / UNKNOWN_VARIANT handling to drop: W3 handled every error generically.
- **Refused video keeps the picture.** A refused video file keeps the projector's last picture (`ProjState::videoKeep`). Before, the
  picture dropped.
- **Undecodable AVFoundation tracks** (found by the forms test, not in the brief). A FourCC no macOS decoder claims (DXV3 / DXT1 /
  YCG6 / DXD1 / DXDA: Arena "not a clip") fell through to AVFoundation, which opened it and then delivered nothing, forever. Now
  `track.decodable` NO refuses it at open (HAP is exempt: it is read as raw samples). Commons LOADEHR-017.
- **HQ shader (kProjVideoFS modes 5 / 6 only).**
  - Chroma midpoint 0.5 (127.5 / 255).
  - Chroma stays nearest.
  - R / B overflow folding exactly as spec 8.4 (R<0, B<0, R>1, B>1, in that order), then the existing clamp.
  - The Hap Q branch was not touched, and its pictures stay byte-identical (below). Commons LOADEHR-016.
- **Tools.**
  - `tools/w8_dxv.py same | forms`.
  - `tools/arena_w8.py`.
  - `tools/w3_dxv.py`: dxvtool now builds per submodule commit (the W3 cache held the 0.1 tool), and `LOADEHR_BUILD=<dir>` renders
    with another build.
- **Docs.** CLAUDE.md DXV section and tool lines updated.

## Proof: HQ vs DXVKit 1.0.0 `dump-rgba` (tools/w3_dxv.py proof; before = the W7 build, after = this build)
| file | variant | before mean / max | after mean / max |
|---|---|---|---|
| resolume-alley testsrc2 1080 (3 frames) | YCG6 | 0.490 / 53-56 | 0.119 / 3 |
| resolume-alley testsrc2 1080 (3 frames) | YG10 | 0.501 / 81-84 | 0.119 / 3 |
| resolume-ame testsrc2 1080 | YCG6 | 0.592 / 53 | 0.119 / 3 |
| resolume-ame testsrc2 1080 | YG10 | 0.622 / 75 | 0.072 / 3 |
| YG10-DXV3.mov (3 frames) | YG10 | 0.41-0.42 / 14-26 | 0.025-0.028 / 1-2 |
| S06 Lone Star Halo 908x2276 (3 frames) | YCG6 | 0.35-0.36 / 2 | 0.010-0.023 / 1 |

After: >= 88 % of values exact and <= 0.0011 % at 3, so max <= 3 is GPU BC4 palette precision, as expected. The DXT1 / DXT5 / legacy rows
(alley, ame, Metric_01, black_snowflake 4K, S08) are identical between before and after, as in W3 (max 1-2 vs dxvtool). The two KLV Set files are still gone (W6).

## Pictures byte-identical to the W7 build (tools/w8_dxv.py same, picture texture dumps, frames both runs drew)
Every frame compared was byte-identical (max 0):

| file(s) | frames |
|---|---|
| alley DXT1 / DXT5 | 3 / 3 |
| ame DXT1 / DXT5 | 1 / 1 |
| Metric_01 (legacy) | 15 |
| black_snowflake 4K DXT5 | 18 |
| S08 DXT5 | 15 |
| HAP | 19 |
| HAP Alpha | 18 |
| **Hap Q** | 18 |
| **Hap Q Alpha** | 18 |
| H.264 | 17 |
| HEVC | 17 |
| ProRes 422 | 17 |
| ProRes 4444 | 17 |

## The 33 DXVKit Arena forms (tools/w8_dxv.py forms)
Method: a playing DXT1 control file first, then the form.

| Arena verdict | forms | LoadEhr |
|---|---|---|
| play (24) | 1-13, 15, 16, 18-23, 27, 28, 30 | plays: pictures drawn, Video Time shows the variant (legacy DXT1 / DXT5, DXT1, YCG6, YG10) |
| black (4) | 14, 17, 29, 31 | refused at frame 0 ("DXV3 header: ..."), control's picture kept (shot mean 0.0000, max 0) |
| not a clip (5) | 24, 25, 26, 32, 33 | refused at open ("<FourCC>: no decoder for this codec on this Mac"), control's picture kept (max 0) |

**33 / 33 agree, exit code 0 on all 33.** The first run, before the `decodable` fix, had the 5 not-a-clip forms reading nothing with the picture dropped.

## Unchanged (checked)
- Regress: all 6 references max 0.
- Guards: G1-G7 ok on all four bundles (LoadEhr 504 / FX 508 / Wow 498 / Wow FX 512 params).
- W4 scrub sweep, DXV 1080 + 4K: 151 / 151 frames shown in order, 0 wrong.
- W5 wire: worst 0 over 48 shots vs verify/w5.
- W6 points renders + W7 transition strips: 76 PNGs max 0 vs verify/w7. The strips run forward = backward, worst 0, on all 48 rows.

## Arena clips (layer 13 "LoadEhr W8", opacity 1, untriggered, selection parked on L13 C6)
Every clip uses the DXV file as its SOURCE with LoadEhr Wow FX on it. The dials are the same (W3 C8 / C9: skull in haze, soft, Spin Y 3),
Input Use Off, set by `tools/arena_w8.py 13`. The files are the Alley testsrc2 vectors in the submodule (saturated bars).
- **C1** W8 YCG6 DXVKit: Proj Source File (DXVKit 1.0.0 decodes in the plugin).
- **C2** W8 YCG6 Arena Native: Proj Source Input (Arena's own decode).
- **C3** W8 YG10 DXVKit / **C4** W8 YG10 Arena Native: the same pair for YG10.

## Deviations
- **Skip a bad frame instead of failing.** "Handle BAD_HEADER like any decode error (keep the last frame)" was done as: a frame-0 failure
  refuses the file, and a later failure skips that frame. W3's "any decode error" stopped the whole video and dropped the picture,
  which kept nothing.
- **The `decodable` refusal is new scope.** It was needed for the forms to agree with Arena (and it stops an endless reader loop).
- The proof is harness-only (GPU picture vs DXVKit's CPU RGBA). There is no Arena capture in this wave; the clip pairs are for your eyes.

## Open / needs Amrit's eyes
- C1 vs C2 and C3 vs C4: does the projected testsrc2 match Arena's own playback in colour, especially the saturated bars?
- The composition was not saved (as always).

## Commons
LOADEHR-016 (DXV HQ on the GPU as Arena shows it) and LOADEHR-017 (an undecodable AVFoundation track reads nothing forever). Synced and pushed.

## RECEIPTS
```
$ git log -1 --format='%H %s' ; git status -sb | head -1     (code commit, before this handoff's commit)
dcebbb8bcbf87185b92e1f0968984f4efbba5b1c LoadEhr W8: DXVKit v1.0.0 (Arena-exact DXV acceptance, HQ colour midpoint 127.5 + R/B folding)
## main...origin/main
$ git -C third_party/dxvkit log -1 --format='%H %d'
2f5256ab2c2b10bdfebe68275129dbf51e48fd10  (HEAD, tag: v1.0.0)
$ dxvtool version   (scratch build of the submodule)
dxvtool 1.0.0 (DXVKit 1.0.0)
$ tools/regress.sh w8/regress
compare verify/w1/ref/r1_mask_front.png vs verify/w8/regress/r1_mask_front.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r2_wireframe50.png vs verify/w8/regress/r2_wireframe50.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r3_explode.png vs verify/w8/regress/r3_explode.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r4_fx_skin_env_input.png vs verify/w8/regress/r4_fx_skin_env_input.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r5_chain_mid_hop_b_c.png vs verify/w8/regress/r5_chain_mid_hop_b_c.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r6_chain_rest_d.png vs verify/w8/regress/r6_chain_rest_d.png: mean 0.0000 max 0 pixels>2 0 of 2073600
G1 ok   LoadEhr: 504 params / LoadEhrFX: 508 / LoadEhrWow: 498 / LoadEhrWowFX: 512, none named Library* / Laser*
G4 ok   (all four) covered pixels 0 ; 4 empty slots ; G6 ok (all four)
G2 ok   the only directory API: ModelLoader.cpp FolderIndex
G3 ok   no model / texture file inside build/LoadEhr*.bundle (8 files: binaries + Info.plist)
G5 ok   no Laser* / Library* sources, no library tools
G6 ok   no Mirror* source, no MIRROR program variant
G7 ok   no libtess2 / tesselator / SGI-B in src, third_party, CMakeLists.txt
G7 ok   no LGPL file in the tree (1130 tracked files, submodules included)
$ tools/w8_dxv.py forms verify/w8/forms | tail -1
files 33 | agree 33 | disagree 0
$ tools/w8_dxv.py same verify/w8/same <W7 build>   (frames | identical | max)
alley dxt1 3|3|0  alley dxt5 3|3|0  ame dxt1 1|1|0  ame dxt5 1|1|0  Metric_01 15|15|0  black_snowflake 18|18|0  S08 15|15|0
hap_1080 19|19|0  hapalpha 18|18|0  hapq 18|18|0  hapqalpha 18|18|0  h264 17|17|0  hevc 17|17|0  prores422 17|17|0  prores4444a 17|17|0
$ tools/w3_dxv.py proof (after, HQ rows: mean / max)
alley ycg6 0.1193/3 0.1192/3 0.1192/3 | alley yg10 0.1195/3 0.1194/3 0.1193/3 | ame ycg6 0.1189/3 | ame yg10 0.0721/3
YG10-DXV3 0.0252/1 0.0275/2 0.0275/2 | S06 YCG6 0.0096/1 0.0168/1 0.0226/1
$ LOADEHR_BUILD=<W7 build> tools/w3_dxv.py proof (before, same rows)
alley ycg6 0.4898/53 0.4903/54 0.4902/56 | alley yg10 0.5009/81 0.5018/84 0.5021/81 | ame ycg6 0.5924/53 | ame yg10 0.6220/75
YG10-DXV3 0.4069/19 0.4154/14 0.4192/26 | S06 YCG6 0.3462/2 0.3533/2 0.3594/2
$ tools/w4_scrub.py sweep (DXV)
| code_dxv_1080 | 151 | 151 (want 151) | yes | 0 in between, 0 wrong | 3 |
| code_dxv_4k | 151 | 151 (want 151) | yes | 0 in between, 0 wrong | 3 |
$ tools/w6_points.py wire_same verify/w8/wire_same
worst: 0 over 48 shots
$ harness compare verify/w7/{strips_final,points_w7}/*.png vs verify/w8/{strips,points}
  76 max 0 pixels>2
$ ls -la ~/Documents/Resolume\ Arena/Extra\ Effects/LoadEhr*.bundle/Contents/MacOS/
-rwxr-xr-x  1 <user>  staff  4621472 Oct  8 22:47 LoadEhr
-rwxr-xr-x  1 <user>  staff  4677456 Oct  8 22:47 LoadEhrFX
-rwxr-xr-x  1 <user>  staff  4621472 Oct  8 22:47 LoadEhrWow
-rwxr-xr-x  1 <user>  staff  4677456 Oct  8 22:47 LoadEhrWowFX
$ Arena: PID 53933 -> 27709 (Amrit quit + relaunched) ; layer 13 "LoadEhr W8" opacity 1
 C1 W8 YCG6 DXVKit Disconnected File proj resolume-alley_ycg6_testsrc2_1920x1080.mov
 C2 W8 YCG6 Arena Native Disconnected Input
 C3 W8 YG10 DXVKit Disconnected File proj resolume-alley_yg10_testsrc2_1920x1080.mov
 C4 W8 YG10 Arena Native Disconnected Input
 composition playing: nothing playing on 13 layers ; selection L13 C6 (empty) ; Wire not touched
$ ls -la verify/w8   (local evidence, gitignored)
forms proof_before proof_after same regress scrub strips points wire_same (+ .txt / .log)
```
END OF HANDOFF w8-dxvkit-v1-results sha256:3ef2d6a9 10717
