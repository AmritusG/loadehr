```
LOADEHR W8 — MOVE TO DXVKIT v1.0.0: ARENA-EXACT DXV ACCEPTANCE AND HQ COLOUR (CHROMA MIDPOINT 127.5) IN THE PROJECTOR — RUN TO FINISHED

MODE: build wave on LoadEhr (this session owns the folder). DXVKit is a read-only pinned submodule (any DXVKit bug is REPORTED, not fixed here). Commit + push at the end.
First: cp -p this brief from ~/claude-handoffs/ into docs/handoffs/.

AMRIT'S WORDS (2026-10-08, verbatim; record in the handoff)
On W7: "Now all transtitions looks amazing!" -> W7 open questions closed as accepted: the Twist / Melt top ends stay as built, the sparkle level the clips use stays (no default change).
Earlier (DXVKit): "Clean room." / "Keep exact."

FACTS (checked)
1. DXVKit tag v1.0.0 = commit 2f5256a (pushed). Its ADOPTION NOTE (~/claude-handoffs/dxvkit_2026-10-08_w6-arena-rules-results.md) lists for a caller: acceptance changes (every legacy type byte, stored HQ packets, DXD4 and FourCC / packet mismatches now decode; DXV3 version byte < 4 and stored flag > 1 now rejected with the new DXVK_ERR_BAD_HEADER; DXVK_GEN_AUTO; the packet decides), the HQ RGBA convention (chroma midpoint 127.5, exact BC4 palettes, R / B overflow folding), and for GPU-path callers: chroma midpoint 127.5 / 255 = 0.5 (not 128 / 255) and nearest chroma.
2. LoadEhr pins cdbb4bc (W3). Its DXV HQ shader (ProjectorShaders.h ~l.56-65) converts YCoCg with Hap Q's formula at scale 1 and a chroma bias of 128 / 255 ("chroma biased by 128 (DXVKit's README)"): that is the pre-v1.0.0 convention and is off by half a step vs Arena. W3 already samples HQ chroma nearest.
3. HAP Q is a different codec with its own spec: its scaled-YCoCg bias stays as the HAP spec defines it. Only the DXV HQ path changes.

READ FIRST (all of it)
The DXVKit v1.0.0 adoption note + CHANGELOG.md + include/dxvkit.h at the tag; LoadEhr CLAUDE.md (DXV + VIDEO ALPHA, W4 scrub rules: DXV is an intra codec), src/Video.mm (RunDxv, status handling, generation argument), ProjectorShaders.h (the DXV HQ + Hap Q conversions), tools/w3_dxv.py (the proof).

IMPLEMENT
1. Bump third_party/dxvkit to tag v1.0.0 (2f5256a). VERSIONS.txt. G7 (no LGPL) still passes.
2. Video.mm: pass DXVK_GEN_AUTO (the packet decides), handle DXVK_ERR_BAD_HEADER like any decode error (keep the last frame, log once with Debug Log), drop any code that only existed for the old UNSUPPORTED / UNKNOWN_VARIANT returns (dead code goes).
3. DXV HQ shader (YCG6 / YG10 only): chroma midpoint 0.5 (127.5 / 255), nearest chroma (as now), and the R / B overflow folding exactly as DXVKit's spec 8.4 / adoption note states. As its own branch / variant if it touches the Hap Q path: Hap Q's maths must stay byte-identical.

BUILD + TEST
1. The W3 proof (tools/w3_dxv.py) re-run against DXVKit v1.0.0's own dump-rgba: every remaining DXV file, all variants, max / mean diff; HQ files must now match v1.0.0's RGBA (expect max <= 2-4 from GPU BC interpolation as in W3; report). DXT1 / DXT5 / legacy: unchanged vs W3 (max 0 between the W7 build and this build).
2. Hap Q, HAP, H.264, ProRes: picture byte-identical to the W7 build (max 0).
3. The DXVKit Arena forms (~/Movies/Testfiles/dxvkit-w5-forms, 33 files) through the projector in the harness: each plays or refuses exactly as Arena does (the arena_forms.tsv verdicts); refused ones keep the last frame, never crash.
4. regress max 0; guards G1-G7 all bundles; W4 scrub sweep (DXV), W5 wire, W6 points, W7 transitions strips unchanged.
5. Install; ask Amrit to quit Arena fully (Arena > Quit Arena, cmd-Q) and relaunch; WAIT; check the PID changed.
6. CLIPS, layer "LoadEhr W8", untriggered, soft, selection parked on an empty slot: the COMPARISON for the HQ colour: (a) a YCG6 file through Proj File (DXVKit v1.0.0) and (b) the same file played natively by Arena as the layer input into Wow FX with Proj Source Input, adjacent, same dials (as W3 C8 / C9); the same pair for YG10. Say which columns.
7. Commit + push. Handoff docs/handoffs/loadehr_2026-10-08_w8-dxvkit-v1-results.md, cp -p to ~/claude-handoffs/: the bump, code changes, proof tables (HQ before / after vs v1.0.0 RGBA, others unchanged), forms agreement table, clip columns, guards, Amrit's answers.

STANDING RULES
One Claude per folder. Cowork runs no git. Never trigger or activate clips; never kill Arena; do not ask Amrit to save the comp. Carnival mask forbidden. No library, no lasers, no mirror room. W14 clocks, W28 ms host clock, W23 alpha, W25 range clamp, 512 params, 16-char names, Apple GL 4.1 (LOADEHR-011: no mid-shader discard). Never use AVAssetReader resetForReadingTimeRanges. Licences MIT / BSD / Apache / zlib / ISC / Boost only. Dead code goes. Copies preserve timestamps. Anything Amrit must do: WHAT / HOW / WHERE.

STOP after the handoff. Report.
```
