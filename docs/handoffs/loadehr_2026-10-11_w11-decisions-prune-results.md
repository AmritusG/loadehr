project:    LoadEhr
pass:       w11-decisions-prune
STATUS:     COMPLETE
parent:     a5b2d37
result:     140d74b
session:    b366879d-8eb1-422f-a356-faf666b10881
written_at: 2026-10-11T02:40:41Z

# LoadEhr W11: Amrit's W10 decisions + the W9 prune (results)

## Amrit's answers (verbatim, 2026-10-11)
"1: ues. 2: yes 3: If a plugin cannot load or use something, remove it. 4: Drop 5: Keep 6: Allow for choosing behaviour"
W9 prune: "E Shape: Sphere and Cube / D Surface: Keep all / C Cut: Keep all except W9 Window Video / B Build: Keep all / A Lines: Keep all".
Also: "Smoke Sim in Wow only." It stays where W9 put it; nothing changed.

## Counts
LoadEhr 494 → **487**, FX 507 → **508**, Wow 504 → **497**, Wow FX 509 → **510** (all ≤ 512).
- Part 1–8 Skin out of the source bundles (−8).
- C Auto Place added in every bundle (+1).

## Stage 1: defaults (calls 1, 2)
Trail Offset X default 0 → 0.02. Ghost Spacing default 5 → 15. Ranges unchanged.

Proof: tools/w11_decisions.py defaults, LoadEhr, skull, each control raised alone. Changed px vs the control at 0; the noise is 0 in every row (docs/w11/defaults.md).

| scene | control | W10 px | W11 px | W10 coverage | W11 coverage |
|---|---|---|---|---|---|
| still | Trail Echoes 4 | 699 | 8,897 | +0 | +8,439 |
| moving | Trail Echoes 4 | 1,312 | 7,321 | +840 | +7,029 |
| moving | Ghosts 4 | 3,889 | 6,701 | +1,768 | +4,009 |

The coverage columns count pixels gained outside the live silhouette. In the map, Trail Echoes and Ghosts are now **VISIBLE ALWAYS**: they show once raised, with no other gate.

## Stage 2: "if a plugin cannot load or use something, remove it" (calls 3, 4)

**How it works:**
- Option syntax in ParamList.inc: `"Name{wow}"` / `"Name{classic}"` = listed in that bundle family only. `~` still means retired everywhere.
- Flag `FXONLY` = a param only the FX bundles carry.
- Element values never move (TOROFIELD-050).
- A value a bundle does not list (a saved clip, REST) reads as the param's default. This one generic alias replaced StyleHere / LiveStyle.
- Style Order Cycle / Random go round the bundle's own list: 17 styles classic, 21 Wow.
- Harness readback:
  - LoadEhr: Transition Style 17 / 20 / 6 → 0, 21 → 21; Shape Target 2 / 3 → 0.
  - Wow: Model Source 2 → 0; Bolt Path 3 → 0.

**The check:** `tools/param_audit.py canuse` (new). Every option or param that needs a group, or the FX input, is checked against each bundle's harness dump: what the host is offered. Result: **W10 build 28 rows → W11 build 0** (docs/audit/canuse_w10.md, canuse.md). Env Source Input (FX) in the source bundles is KEPT (your call 5).

| bundle | control / option | why it cannot use it | proof |
|---|---|---|---|
| LoadEhr, FX | Transition Style Swarm | no Swarm group | W10 sweep: 0 px vs Dissolve (the code dissolved it) |
| LoadEhr, FX | Transition Style Smoke | Smoke Sim is Wow only (W9) | W10 sweep: 0 px vs Dissolve |
| LoadEhr, FX | Transition Style Burn | no Fire group | W10 sweep: 0 px vs Dissolve |
| LoadEhr, FX | Transition Style Lava | no Lava group | W10 sweep: 0 px vs Dissolve |
| Wow, Wow FX | Model Source Logo | no Logo group: the logo file is empty, the load fails, the old model stays | static: TextLogoSpec reads Logo File, not carried there |
| Wow, Wow FX | Bolt Path To Clones (**retired everywhere**: its clone branch is deleted) | no clones (Copies is classic) | W10 sweep: 0 px vs Crawl; static: `fs.inst.size() < 2` → path 0 |
| LoadEhr, Wow | Part 1–8 Skin (8 params) | needs the FX input layer (it was carried but hidden) | static: read only under Input Use Skin, an FX-only group |

Re-checked and kept, as the brief says (usable: untested content or noise): Height Displace, Inside Heat, Face Smooth, the Wow FX smoke dials.

## Stage 3: C Auto Place (call 6)
New OPT in the Light C group: **Off / Once Per Model (default = W10) / Position Only / Every Model Swap**. The C Place At Eyes and C Fit Cone buttons still place in every mode. No option is identical to another in code.

Test: tools/w11_decisions.py lightc (docs/w11/lightc.md). LoadEhr, skull in slot A. The C dials were set first: Aim Free, Cone 70, Behind 1.5, not anchored. Then C Enable, the finder lands, and the chain hops to a second model (Damaged Helmet, test_models). Values read back with `get`:

| option | dials preset → after finder (skull) | → after hop (helmet) | positions set by hand first |
|---|---|---|---|
| Off | nothing written | nothing | nothing |
| Once Per Model | positions (−0.274, 0.112, 0.702), aim Through Hole, cone 40, behind 0.157, anchor on | unchanged (glide only) | left alone (W10 rule) |
| Position Only | positions written; aim Free / cone 70 / behind 1.5 / anchor off kept | unchanged | left alone |
| Every Model Swap | as Once | re-placed for the helmet: (−0.511, 0.266, 0.466), behind 0.035 | overwritten |

- **Off and the Window:** with Off, the Window uses the finder's own openings and writes no dial. Window Off vs Once: 68 px, max 61 (Once's right eye is Mirror X's mirror of the left; Off uses each found eye).
- **Every Model Swap at a hop:** the openings do not jump (the glide is re-based on the newly placed model). The cone / Behind Depth re-fit is one step. Beam-only (models hidden), the per-step change at the swap is mean 2.10, vs 1.08 for Once over the same step.

## Stage 4: the W9 prune
**Shape Target:** Torus and Crystal retired; their values read as Sphere. `ShapeTarget()` is now the cube only. tools/w9_shape.py same vs the W10 build (docs/w11/shape_same.md), skull and text, Spherize 0.3 / 0.7 / 1: **Sphere max 0 (6 / 6), Cube max 0 (6 / 6)**.

**Cut Window:** the projector / video world is gone. Removed: winPicture, winQuad, winSampler, windowPic, and kWinQuad's picture mode with its uniforms. The Wow world is the first other filled slot (docs/w11/window.md):

| case | result |
|---|---|
| LoadEhr Window (Slot B world), W11 vs W10 | **max 0** |
| Wow Window, projector on (aimed away) vs off, W10 | 4,970 px, max 181 (the video in the eyes) |
| Wow Window, projector on vs off, W11 | **max 0** |
| Wow Window, projector off, W11 vs W10 | max 0 |

## Audit (tools/param_audit.py all, plus fresh re-sweeps)
**DEAD 0, BROKEN 0, MISROUTED 0; canuse left 0.**

No visible effect (accepted):
- LoadEhr 1, FX 1: Height Displace.
- Wow 2: + Inside Heat.
- Wow FX 3: + Ring Smoke, the chaotic sim, already accepted in W10.

**Deviation 1 (the BROKEN rows):**
- The first full sweep reported BROKEN 91 in the stateful / chaotic groups (sim, trip, lightning, swarm, camera cuts, dust). Cause: `all` sweeps without `--fresh`; W10 had swept those 70 / 79 / 175 / 175 params fresh.
- Re-swept fresh: 11 left. Swept again one at a time (AUDIT_JOBS=1): 10 visible. None of those 10 params' code changed.
- Fire Smoke (112 px, at noise 0, under the 130 px rule; W10 read 165): its recipe now adds Smoke Ambient 1. Then visible.

## Arena
- **Restart:** Arena PID 36261 → **80139** after your cmd-Q relaunch. All four bundles were installed and ad-hoc signed; their binaries are byte-identical to the final build/.
- **REST name check vs harness**, same order and identical option lists:

| bundle | names | note |
|---|---|---|
| LoadEhr | 405 / 405 | |
| LoadEhr Wow | 398 / 398 | Model Source: Model File / Text; Bolt Path: 3 options |
| LoadEhr FX | 418 / 418 | + Arena's Opacity |
| LoadEhr Wow FX | 403 / 403 | + Arena's Opacity |

- **NEW layer 20 "LoadEhr W11 Decisions"**, all clips Disconnected (never triggered); selection parked on L20 C30:

| col | clip | bundle |
|---|---|---|
| C1 | Trail Echoes 4 (new defaults) | LoadEhr |
| C2 | Ghosts 4 (new defaults) | LoadEhr |
| C3 | C Auto Place Off | LoadEhr |
| C4 | C Auto Place Once Per Model | LoadEhr |
| C5 | C Auto Place Position Only | LoadEhr |
| C6 | C Auto Place Every Model Swap | LoadEhr |
| C7 | Cut Window Wow Slot World (Proj Enable on with the DXV file; the window shows the SciFi helmet) | LoadEhr Wow |
| C8 | FX Map Check LoadEhr FX | added for the name check, as in W10 |
| C9 | FX Map Check Wow FX | added for the name check, as in W10 |

  - Every clip: the skull with Spin Y 20.
  - C3–C6: skull → Damaged Helmet Ping-Pong (4 s, hold 3 s), C dials preset to Free / yaw 40 / cone 60 / behind 1.2.
  - `tools/arena_w11.py 20` re-dials them.
- **Earlier layers:** not touched.
- **After my release (lock at ~01:36):** Arena was relaunched at 01:59 (now PID 19729). No crash report. Fanous W2 holds the lock since 01:45. Layer 20 is intact after that relaunch.

## Needs your eyes
1. Select C3–C6 one at a time (no trigger needed): Off leaves the beams at the preset aim; Position Only puts them in the eyes but keeps yaw 40 / cone 60; Every Model Swap re-fits at each hop.
2. C7: the window shows the helmet even with the projector on.
3. Every Model Swap re-fits cone / depth at each hop's midpoint: one visible step in the beam. Fine for you, or should it glide too?

## Paths
- **Code:** src/ParamList.inc, src/LoadEhrPlugin.cpp, src/Build.cpp, src/Projector.cpp, src/Renderer.h, src/Shaders.h.
- **Tools:** tools/param_audit.py (canuse, option tags, FXONLY), tools/w11_decisions.py, tools/arena_w11.py, tools/param_map.py (per-bundle options), tools/w9_shape.py.
- **Docs:** docs/w11/*.md, docs/audit/canuse*.md, docs/audit/arena_rest_W11_*.json, docs/param_hierarchy.md, CLAUDE.md (W11 section and param map).
- **Commit:** 140d74b (pushed).

## RECEIPTS
```
$ git log -1 --format="%H %s" ; git status -sb | head -1
140d74b45caf68552c07e9339453451249acb0fa LoadEhr W11: Amrit's W10 decisions + the W9 prune
## main...origin/main

$ tools/regress.sh w11   (final build/)
ref-hash guard ok: 6 references match tools/regress_refs.sha256
compare verify/w1/ref/r1_mask_front.png vs verify/w11/r1_mask_front.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r2_wireframe50.png vs verify/w11/r2_wireframe50.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r3_explode.png vs verify/w11/r3_explode.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r4_fx_skin_env_input.png vs verify/w11/r4_fx_skin_env_input.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r5_chain_mid_hop_b_c.png vs verify/w11/r5_chain_mid_hop_b_c.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r6_chain_rest_d.png vs verify/w11/r6_chain_rest_d.png: mean 0.0000 max 0 pixels>2 0 of 2073600
G1 ok   LoadEhr: 487 params, none named Library* / Laser*
G4 ok   LoadEhr: covered pixels 0 ; 4 empty slots; get Status A = 'Nothing loaded: transparent output. ...'
G1 ok   LoadEhrFX: 508 params, none named Library* / Laser*
G1 ok   LoadEhrWow: 497 params, none named Library* / Laser*
G1 ok   LoadEhrWowFX: 510 params, none named Library* / Laser*
G2 ok   the only directory API: ModelLoader.cpp FolderIndex (textures next to the chosen model file)
G3 ok   no model / texture file inside build/LoadEhr*.bundle (8 files: binaries + Info.plist)
G5 ok   no Laser* / Library* sources, no library tools
G6 ok   no Mirror* source, no MIRROR program variant   (and G6 per bundle x4 ok, G4 per bundle x4 ok)
G7 ok   no libtess2 / tesselator / SGI-B in src, third_party, CMakeLists.txt
G7 ok   no LGPL file in the tree (1166 tracked files, submodules included)

$ tools/param_audit.py map / canuse (after the fresh re-sweeps)
LoadEhr      VISIBLE ALWAYS 111 | NON-VISUAL 12 | ONLY WHEN MOVING 40 | ONLY WHEN 323 | NO VISIBLE EFFECT 1
LoadEhrFX    VISIBLE ALWAYS 113 | NON-VISUAL 12 | ONLY WHEN MOVING 43 | ONLY WHEN 339 | NO VISIBLE EFFECT 1
LoadEhrWow   VISIBLE ALWAYS 86  | NON-VISUAL 12 | ONLY WHEN MOVING 30 | ONLY WHEN 367 | NO VISIBLE EFFECT 2
LoadEhrWowFX VISIBLE ALWAYS 87  | NON-VISUAL 12 | ONLY WHEN MOVING 30 | ONLY WHEN 378 | NO VISIBLE EFFECT 3
DEAD 0 BROKEN 0 MISROUTED 0
canuse: 66 offered rows checked, 2 kept by Amrit, CANNOT USE left: 0
(W10 build, AUDIT_BUILD: canuse: 94 offered rows checked, 2 kept by Amrit, CANNOT USE left: 28)

$ build/ vs installed binaries (pre-signature)
LoadEhr same / LoadEhrFX same / LoadEhrWow same / LoadEhrWowFX same

$ ls -la docs/w11 docs/audit (selection)
-rw-r--r--  680 Oct 11 01:25 docs/w11/defaults.md
-rw-r--r-- 3258 Oct 11 01:22 docs/w11/lightc.md
-rw-r--r--  393 Oct 11 01:16 docs/w11/shape_same.md
-rw-r--r--  629 Oct 11 01:19 docs/w11/window.md
-rw-r--r--   6864 Oct 11 04:38 docs/audit/canuse.md
-rw-r--r--  11091 Oct 11 01:14 docs/audit/canuse_w10.md
-rw-r--r-- 103423 Oct 11 01:35 docs/audit/arena_rest_W11_L20C1_LoadEhr.json
-rw-r--r-- 102526 Oct 11 01:35 docs/audit/arena_rest_W11_L20C7_LoadEhrWow.json
-rw-r--r-- 118866 Oct 11 01:35 docs/audit/arena_rest_W11_L20C8_LoadEhrFX.json
-rw-r--r-- 115959 Oct 11 01:35 docs/audit/arena_rest_W11_L20C9_LoadEhrWowFX.json
-rw-r--r-- 421727 Oct 11 04:28 docs/audit/sweep_LoadEhr.json   (+ FX 441006, Wow 433204, Wow FX 444109)

$ Arena 7.28: PID 36261 -> 80139 (restart by Amrit, 01:35); later 19729 (relaunched 01:59 after my lock release; no DiagnosticReport)
Layer 20 "LoadEhr W11 Decisions": C1..C9 all Disconnected; selection parked on L20 C30.
REST vs harness: LoadEhr 405/405, Wow 398/398, FX 418/418, Wow FX 403/403 names, same order, 0 option-list differences.
```
END OF HANDOFF w11-decisions-prune sha256:481a4e88 12985
