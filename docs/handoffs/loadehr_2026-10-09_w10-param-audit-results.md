project:    LoadEhr
pass:       w10-param-audit
STATUS:     COMPLETE
parent:     2f90f3a
result:     15cb464
session:    616aec52-0a13-4b00-b612-b13f2758f3ae
written_at: 2026-10-09T14:47:02Z

# LoadEhr W10: param audit (results)

## Verdict on Cowork's suspicion
**Gating, not breakage:** every Copies control is wired and changes the picture once its gates are open (31 / 31, noise 0). The defaults hide it: Trail Echoes 0 and Ghosts 0 switch off everything after LOD Bias. Trail Offset 0 leaves the echoes under the live mask. At Ghost Spacing 5 the ghosts stay inside the live silhouette. Separately, the whole-plugin sweep and a read of the code found 3 real bugs elsewhere, each partial; all 3 are fixed (below).

## Counts (four bundles; the sweep on the W9 code, fixes proven separately)
| bundle | params | visible on its own | only when moving | only with gates | non-visual by design | no visible effect (accepted) | dead | broken | misrouted |
|---|---|---|---|---|---|---|---|---|---|
| LoadEhr | 494 | 110 | 41 | 322 | 12 | 9 | 0 | 0 | 0 |
| LoadEhr FX | 507 | 112 | 44 | 337 | 12 | 2 | 0 | 0 | 0 |
| LoadEhr Wow | 504 | 86 | 30 | 366 | 12 | 10 | 0 | 0 | 0 |
| LoadEhr Wow FX | 509 | 87 | 30 | 374 | 12 | 6 | 0 | 0 | 0 |

- **Non-visual by design (12):** Status A–D, Reload A–D (a reload of the same file), Debug Log, Pose Data, C Placed For, Video Time and the Audio FFT buffer.
- **No visible effect (accepted), with reasons in the map:**
  - Height Displace: no test model has a Height map.
  - Part 1–8 Skin in the source bundles: hidden there; needs the FX input.
  - Inside Heat: inside smoke stays in the closed cavity. With the mask hidden: 15.6k px vs 15.0k px noise.
  - Face Smooth: needs a moving face; the input was a still photo.
  - 4 Wow FX smoke dials: within the chaotic sim's run-to-run noise there; the same code reads visible in Wow.
- **Gate counts:** the "only with gates" column is what each param needs before it shows. The exact parent values are in docs/param_hierarchy.md.

## Copies after LOD Bias (scene G = skull moving + gates; changed px at the max-effect value, noise 0 for all)
| control | acts on | needs | px |
|---|---|---|---|
| Trail Echoes | echoes | the model moving or Trail Offset ≠ 0 | 22,364 |
| Trail Delay | echoes | Echoes > 0 + offset | 21,992 |
| Trail Decay | echoes | Echoes > 0 + offset | 22,552 |
| Trail Offset X / Y | echoes | Echoes > 0 | 46,515 / 14,519 |
| Ghosts | ghosts | motion; Spacing large enough to leave the silhouette | 8,883 |
| Ghost Spacing | ghosts | Ghosts > 0 + motion | 10,391 |
| Ghost Fade | ghosts | Ghosts > 0 + motion | 12,251 |
| Trail Smear | both | Echoes or Ghosts | 25,938 |
| Trail Blur | echoes | Echoes > 0 + offset (aged pixels only) | 3,795 |
| Trail Dust | both | Echoes or Ghosts | 26,584 |
| Trail Grain | both | + Trail Dust > 0 | 14,138 |
| Trail Drift | both | + Trail Dust > 0 | 26,858 |
| Trail Drift Dir | both | + Dust > 0, Drift > 0 | 65,953 |
| Trail Fade Curve | both | Echoes or Ghosts | 30,760 |
| Trail Glow | both | Echoes or Ghosts | 37,081 |
| Trail Color / Sat / Bri / Alpha | both | + Trail Glow > 0 or Trail Color Mix > 0 | 34,771 / 36,228 / 37,243 / 36,945 |
| Trail Color Mix | both | Echoes or Ghosts | 32,118 |

Clone Layout plus the 9 controls before LOD Bias are also all visible (Clone Layout ≠ Off).

## Fixes (each: changed px for value B vs A, against A vs A noise; tools: fixes.sh in RECEIPTS)
- **Surf Amount 1..2 (Skin Of Copies) did not dissolve the model.** BuildSurfLook set the dissolve, then the FX block overwrote it. Before: 23,221 = noise (23,221). After: 31,308.
- **Cut Look Window switched off Shatter / Voxel / Low Poly / letters / Lava Split.** BuildCut wrote `shape.on = deform || cut`.
  - Voxel before: 11,354 = noise. After: 25,318.
  - Low Poly before: 11,354 = noise. After: 27,960.
- **Line Audio did nothing in Light Paint.** The ribbon program got no FFT bands. Now light and trail length follow each band, as Plexus and Neon already did. Before: 14,637 = noise. After: 21,633.
- **W9 backlog:** the light-paint ribbon program is now prewarmed.
  - Print On's 53 ms W9 frame did not reproduce. Harness, cold process: worst 4.4 ms after the switch.
  - Arena, first second after a fresh restart: Print On cpu 0.76 ms avg, p95 3.25; Light Paint cpu 0.73, p95 2.11. The extra render time in that second (about 130 ms) is the model swap and its prewarm.
- **Gates:** plain programs are byte-identical (regress 6/6 max 0), guards G1–G7 ok, params 494 / 507 / 504 / 509 (all ≤ 512).

## Proposals and taste calls (one line each; nothing changed without you)
1. **Trail Offset X default 0 → 0.02**, so Trail Echoes shows on its own. Stills: docs/audit/proposals/proposals.png (top row).
2. **Ghost Spacing default 5 → 15**, so Ghosts leave the live silhouette. Same PNG, bottom row.
3. **Model Source "Logo" in the Wow bundles:** there is no Logo group there, so a pick fails and keeps the old model. Hide it there, or leave it?
4. **Bolt Path "To Clones" in the Wow bundles** behaves like Crawl, because there are no clones there. Keep or drop?
5. **Env Source "Input (FX)" in the source bundles** gives the same picture as Studio. Keep?
6. **Light C:** C Enable auto-places once per model when the eye finder lands, and overwrites C dials set before that. Keep as is, or skip writing Aim Mode?

## Arena (read only + one new untriggered layer)
- **Restart:** Arena was restarted (PID 15450 → 43449).
- **New layer 19 "LoadEhr W10 Copies":** C1 Trail Echoes, C2 Trail Delay, C3 Trail Decay, C4 Trail Offset X, C5 Trail Offset Y, C6 Ghosts, C7 Ghost Spacing, C8 Ghost Fade, C9 Trail Smear, C10 Trail Blur, C11 Trail Dust, C12 Trail Grain, C13 Trail Drift, C14 Trail Drift Dir, C15 Trail Fade Curve, C16 Trail Glow, C17 Trail Color (the quartet as one picker), C18 Trail Color Mix, C19 FX Map Check (Solid Color + LoadEhr FX).
  - Every clip: the skull with Spin Y 20 and Wobble 1, its gates open, the control pushed. `tools/arena_w10.py 19` re-dials them.
- **REST names vs the harness:**
  - LoadEhr FX: 417 / 417 names in the same order.
  - LoadEhr: identical (404 + the part slots once a model has rendered).
  - All 18 Copies names are present in both.
- **Left as found:** Debug Log on L15 C2 and L14 C2 was turned on for the PERF read and set back off. The selection is parked on L19 C30.

## Paths
docs/param_hierarchy.md (the map) · docs/audit/ (static.json / .md, sweep_<bundle>.json, the Copies contact sheets copies_LoadEhr*_sheet_[ab].png, proposals/, Arena REST dumps) · tools/param_audit.py, tools/param_audit_gates.json, tools/arena_w10.py · Commons LOADEHR-022 (the sweep's five traps), LOADEHR-023 (Apple GL: no cross-process shader cache to defeat).

## RECEIPTS
```
$ git log -1 --format="%H %s" ; git status -sb | head -1
15cb464d6f591f81399f1b180dd6bea2dc774604 LoadEhr W10: param audit (every control: carried / read / visible, gates mapped) + 3 fixes
## main...origin/main

$ grep -E "^compare" verify/w10/regress.log
compare verify/w1/ref/r1_mask_front.png vs verify/w10/r1_mask_front.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r2_wireframe50.png vs verify/w10/r2_wireframe50.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r3_explode.png vs verify/w10/r3_explode.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r4_fx_skin_env_input.png vs verify/w10/r4_fx_skin_env_input.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r5_chain_mid_hop_b_c.png vs verify/w10/r5_chain_mid_hop_b_c.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r6_chain_rest_d.png vs verify/w10/r6_chain_rest_d.png: mean 0.0000 max 0 pixels>2 0 of 2073600

$ tools/guards.sh (W10 build)
G1 ok   LoadEhr: 494 params, none named Library* / Laser*
G6 ok   LoadEhr: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhr: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G1 ok   LoadEhrFX: 507 params, none named Library* / Laser*
G6 ok   LoadEhrFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G1 ok   LoadEhrWow: 504 params, none named Library* / Laser*
G6 ok   LoadEhrWow: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWow: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G1 ok   LoadEhrWowFX: 509 params, none named Library* / Laser*
G6 ok   LoadEhrWowFX: no Mirror Room / Mirror Count / Box Size / LED* param
G4 ok   LoadEhrWowFX: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. Point Model A (or B, C, D) at a model file'
G2 ok   the only directory API: ModelLoader.cpp FolderIndex (textures next to the chosen model file):
G3 ok   no model / texture file inside build/LoadEhr*.bundle (8 files: binaries + Info.plist)
G5 ok   no Laser* / Library* sources, no library tools
G6 ok   no Mirror* source, no MIRROR program variant
G7 ok   no libtess2 / tesselator / SGI-B in src, third_party, CMakeLists.txt
G7 ok   no LGPL file in the tree (1165 tracked files, submodules included)

$ ls -la docs/audit docs/audit/proposals
docs/audit:
drwxr-xr-x  16 <user>  staff      512 Oct  9 16:45 .
drwxr-xr-x   9 <user>  staff      288 Oct  9 16:32 ..
-rw-r--r--   1 <user>  staff    70005 Oct  9 15:35 arena_rest_L14C1_LoadEhr.json
-rw-r--r--   1 <user>  staff    69405 Oct  9 16:42 arena_rest_L19C14_LoadEhr.json
-rw-r--r--   1 <user>  staff    72496 Oct  9 16:42 arena_rest_L19C19_LoadEhrFX.json
-rw-r--r--   1 <user>  staff  1743861 Oct  9 16:38 copies_LoadEhrFX_sheet_a.png
-rw-r--r--   1 <user>  staff  1982333 Oct  9 16:38 copies_LoadEhrFX_sheet_b.png
-rw-r--r--   1 <user>  staff  1743861 Oct  9 14:57 copies_LoadEhr_sheet_a.png
-rw-r--r--   1 <user>  staff  1982333 Oct  9 14:57 copies_LoadEhr_sheet_b.png
drwxr-xr-x   3 <user>  staff       96 Oct  9 16:45 proposals
-rw-r--r--   1 <user>  staff   441777 Oct  9 14:40 static.json
-rw-r--r--   1 <user>  staff    81969 Oct  9 14:40 static.md
-rw-r--r--   1 <user>  staff   368839 Oct  9 16:30 sweep_LoadEhr.json
-rw-r--r--   1 <user>  staff   378729 Oct  9 16:35 sweep_LoadEhrFX.json
-rw-r--r--   1 <user>  staff   385113 Oct  9 16:30 sweep_LoadEhrWow.json
-rw-r--r--   1 <user>  staff   387377 Oct  9 16:35 sweep_LoadEhrWowFX.json

docs/audit/proposals:
drwxr-xr-x   3 <user>  staff       96 Oct  9 16:45 .
drwxr-xr-x  16 <user>  staff      512 Oct  9 16:45 ..
-rw-r--r--   1 <user>  staff  1234209 Oct  9 16:44 proposals.png

$ tools/param_audit.py map (counts)
{ "LoadEhr": { "VISIBLE ALWAYS": 110, "NON-VISUAL": 12, "VISIBLE ONLY WHEN MOVING": 41, "VISIBLE ONLY WHEN": 322, "NO VISIBLE EFFECT": 9 }, "LoadEhrFX": { "VISIBLE ALWAYS": 112, "NON-VISUAL": 12, "VISIBLE ONLY WHEN MOVING": 44, "VISIBLE ONLY WHEN": 337, "NO VISIBLE EFFECT": 2 }, "LoadEhrWow": { "VISIBLE ALWAYS": 86, "NON-VISUAL": 12, "VISIBLE ONLY WHEN MOVING": 30, "VISIBLE ONLY WHEN": 366, "NO VISIBLE EFFECT": 10 }, "LoadEhrWowFX": { "VISIBLE ALWAYS": 87, "NON-VISUAL": 12, "VISIBLE ONLY WHEN MOVING": 30, "VISIBLE ONLY WHEN": 374, "NO VISIBLE EFFECT": 6 }}
DEAD 0BROKEN 0MISROUTED 0

$ fixes.sh build/ (W10, after) vs the W9 build (before): changed px, value B vs A | A vs A (noise)
AFTER (build/, W10):
  surf_amount_1_vs_2_noise changed 23221
  surf_amount_1_vs_2 changed 31308
  window_voxel_0_vs_1_noise changed 11354
  window_voxel_0_vs_1 changed 25318
  window_lowpoly_0_vs_1_noise changed 11354
  window_lowpoly_0_vs_1 changed 27960
  lightpaint_audio_0_vs_4_noise changed 14637
  lightpaint_audio_0_vs_4 changed 21633
  plexus_audio_0_vs_4_noise changed 4388
  plexus_audio_0_vs_4 changed 14659
BEFORE (the W9 build, same script, run 2026-10-09 ~15:05 before the fixes were built):
  surf_amount_1_vs_2_noise changed 23221 | surf_amount_1_vs_2 changed 23221
  window_voxel_0_vs_1_noise changed 11354 | window_voxel_0_vs_1 changed 11354
  window_lowpoly_0_vs_1_noise changed 11354 | window_lowpoly_0_vs_1 changed 11354
  lightpaint_audio_0_vs_4_noise changed 14637 | lightpaint_audio_0_vs_4 changed 14637
  plexus_audio_0_vs_4_noise changed 4388 | plexus_audio_0_vs_4 changed 14659   (control: Plexus had audio before and after)

$ Arena 7.28 (PID 15450 -> 43449 after Amrit's restart), PERF lines, clip SELECTED not triggered, 1920x1080
16:43:23 PERF W9 Print On  n 85 fps 105.6 | cpu ms 0.76 p95 3.25 | render ms 2.58   (first second: model swap + prewarm)
16:43:24 PERF W9 Print On  n 88 fps 91.3  | cpu ms 1.02 p95 1.47 | render ms 1.05
16:43:43 PERF W9 Light Paint n 98 fps 97.2 | cpu ms 0.73 p95 2.11 | render ms 1.78  (first second)
16:43:44 PERF W9 Light Paint n 86 fps 87.6 | cpu ms 1.32 p95 2.26 | render ms 1.46
Layer 19 "LoadEhr W10 Copies": 19 clips, all Disconnected (untriggered); selection parked on L19 C30 (empty).
REST vs harness: LoadEhr FX (L19 C19 effect) 417 / 417 names, 0 order differences; LoadEhr (L14 C1, model loaded) 404 + Part 1 x4, 0 differences.
```
END OF HANDOFF w10-param-audit sha256:8ce392a9 13078
