project:    loadehr
pass:       go-public
STATUS:     BLOCKED
parent:     1023554
result:     dc9a56c
session:    6d76d8dd-7b02-43ab-a345-b35e8d1c7c6a
written_at: 2026-10-17T18:28:15Z

# LoadEhr: go public (results, BLOCKED at the visibility flip) -- r2

r2 corrects r1 (loadehr_2026-10-17_go-public-results_BLOCKED.md, removed from the repo in the same commit): r1 quoted the
last name 9 times, which broke this pass's own gate. Content otherwise unchanged.

## Amrit's three answers (2026-10-17)
- Licence: "MIT (Recommended)".
- Full last name in file paths: "Scrub current files (Recommended)" (current files only; history NOT rewritten).
- DXVKit (private submodule): "Make DXVKit public".

## Where it stands
- Stages 1-3 DONE, committed + pushed: dc9a56c (main = origin/main).
- Stage 4 (DXVKit): REPORTED, NOT flipped. Its tracked files hold the user name (details below), so the brief's rule applies.
- Stage 5 (LoadEhr public): BLOCKED. `gh repo edit AmritusG/loadehr --visibility public ...` was refused by Claude Code's
  auto-mode permission classifier ("Create Public Surface"); a read-only curl of the GitHub page was refused the same way.
  I did not work around it. Both repos are still PRIVATE.
- URLs once flipped: https://github.com/AmritusG/loadehr and https://github.com/AmritusG/dxvkit

## What Amrit needs to do (WHAT / HOW / WHERE)
- WHAT: make LoadEhr public. HOW: in this Claude Code session type
  `! gh repo edit AmritusG/loadehr --visibility public --accept-visibility-change-consequences`
  (or GitHub > AmritusG/loadehr > Settings > General > Danger Zone > Change visibility > Public).
- Until DXVKit is public too, a logged-out `git clone --recurse-submodules` of LoadEhr clones LoadEhr but FAILS on
  third_party/dxvkit (private), so the project cannot be built from a public clone. README's build steps assume both public.
- Then ask the codecs Cowork to scrub DXVKit (list below) and flip it:
  `gh repo edit AmritusG/dxvkit --visibility public --accept-visibility-change-consequences`.
- Verification still owed after the flips: curl -s -o /dev/null -w "%{http_code}" on both URLs = 200; a fresh
  `git clone --recurse-submodules` into a temp folder without credentials succeeds (then delete it).

## Stage 1: pre-public check (LoadEhr)
- 240 tracked files (+3 new). No model, texture, video or font is tracked, now or in history. Tracked pictures:
  test_images/projector_test.png (ours: generated test card, MaskEhr W26 handoff "our generated test picture"; looked at it:
  colour field, rings, cross, "WOW"), test_logos/test_emblem.svg/.pdf/.png (ours: tools/w21_test_logos.py),
  test_logos/w2/*.svg (ours: fill-rule test shapes). Written up in docs/ASSETS.md.
- test_media/ is gitignored (only SOURCE.txt and the CC0 photo + a derived still, local only).
- Secrets: none. Tracked files and the whole history (`git log -p --all`, outside third_party) scanned for passwords,
  tokens, API keys, private keys, ghp_ / github_pat_ / AKIA patterns, notarytool / keychain-profile / app-specific
  passwords; history file names for .p12 / .cer / .pem / .key / .env / credential. Only false positives (a "one token"
  harness comment, a brief sentence).
- Worth knowing (not a stop): HISTORY (not the current tree) still holds the renders removed in 90e4e1b (docs/parity/*.jpg,
  docs/audit/*.png): renders of third-party models (MaskEhr library skulls / helmets / Plague Mask). The carnival-mask ban
  was already in the very first commit and parity.py refused it then, so none should show it; I did not open every image.
  History also carries the full name + the gmail address as commit author on every commit (git metadata), and the
  removed /Users/<user> paths. All by Amrit's choice (no history rewrite).

## Stage 2: licence
- LICENSE = MIT, byte-identical to third_party/dxvkit/LICENSE (holder line = the full name, as in DXVKit).
- README.md: what LoadEhr is (four bundles, IDs), build (macOS 11+, universal, CMake 3.20+), install, licence, third-party
  notices (cgltf MIT, clipper2 Boost, earcut ISC, FFGL BSD, meshoptimizer MIT, nanosvg zlib, stb / ufbx MIT-or-PD,
  dxvkit MIT). No feature promises.

## Stage 3: scrub (48 files + harness)
- Docs / handoffs / CLAUDE.md: /Users/<user>/ -> ~/ ; `ls -la` owner column -> <user> ; scratch folder names
  -Users-<user>- ; pasted `git log` author lines -> "AmritusG <email omitted>".
- Harness scripts (tools/scripts/*.txt) and tools/param_audit_gates.json: paths -> ~/... ; the harness now expands " ~/"
  in a script line to $HOME (tools/harness/harness.cpp; proven: `text Model_A ~/.../Human Skull/scene.gltf` loaded and
  swapped, 31016 tris). param_audit's map writer treats "~" paths like "/" ones.
- Python tools (w2_deform, w5_wire, w3_dxv, w4_scrub, w2_video, param_audit, arena_w5): os.path.expanduser;
  parity.py's stale scratch default -> tempfile.gettempdir()/mehr_build (MASKEHR_BUILD still overrides).
- DEVIATION (judgement, flag for Amrit): the last name was also in user-visible identifiers, so the plugin about string
  "LoadEhr by <full name>" -> "LoadEhr by AmritusG" (4 bundles + 2 probes) and CFBundleIdentifier com.<user name>.* ->
  com.amritusg.* (4 bundles + 2 probes). Arena keys plugins by the FFGL IDs (LEhr / LEfx / LEwo / LEwf) and params by
  name, so saved clips are unaffected; renders are unchanged (regress max 0). The INSTALLED bundles in Extra Effects still
  carry the old strings until the next install (no Arena this wave). Revert = one sed if he prefers the full name there.
- Result: `git grep -i <last name>` = LICENSE:1 only.

## Stage 4: DXVKit (read only, nothing edited)
- 812 tracked files at 2f5256a (v1.0.0). Secrets: none. Media: test_vectors/source/*.png (our patterns), streams from
  dxvkit / glenc / synthetic, resolume-alley / resolume-ame = FFmpeg testsrc2 encoded with Alley / AME (rule 6 allows
  testsrc2). No clips of Amrit's, no Resolume demo content.
- FOR THE CODECS COWORK (why it was not flipped):
  * the user name (ls owner column) in 3 handoffs: dxvkit_2026-10-08_w3-alley-fair-compare-r2.md (4 lines),
    dxvkit_2026-10-08_w3-alley-fair-compare.md (4), dxvkit_2026-10-08_w3-encoder-results.md (13).
  * the full name outside copyright lines: CLAUDE.md:5, w1-spec-brief.md:4, w1-spec-results.md:12 + :79 (git log line),
    w2-cleanroom-results.md:109, w3-encoder-results.md:203 (git log lines).
  * Copyright headers "MIT License. Copyright (c) 2026 <full name>" in ~33 source files + LICENSE: licence lines,
    fine under the LoadEhr rule (LICENSE excepted); whether source headers count is the codecs Cowork's call.
  * Then flip it (command above) and bump nothing in LoadEhr (the submodule pin 2f5256a stays valid if the scrub is a new
    commit; LoadEhr keeps pointing at v1.0.0).

## Commons
- No finding written: nothing here is an FFGL / Arena behaviour, cost, licence ruling or dead end beyond the repo itself.

## RECEIPTS
```
$ git log -1 --format='%H %d%n%an %ad%n%s' --date=iso      (author name shown as the handle)
dc9a56c51c225b8a6ea562660ff636ad13a6e29f  (HEAD -> main, origin/main, origin/HEAD)
AmritusG 2026-10-17 20:25:23 +0200
Go public: MIT LICENSE, README, docs/ASSETS.md; full last name scrubbed from current files
$ git status -sb
## main...origin/main
$ git grep -c -i <last name>
LICENSE:1
$ ls -la LICENSE README.md docs/ASSETS.md
-rw-r--r--  1 <user>  staff  1069 Oct 17 20:23 LICENSE
-rw-r--r--  1 <user>  staff  1356 Oct 17 20:23 README.md
-rw-r--r--  1 <user>  staff  1038 Oct 17 20:23 docs/ASSETS.md
$ cmake --build build -j
[ 65%] Built target LoadEhrWow
[ 76%] Built target LoadEhr
[ 88%] Built target LoadEhrWowFX
[100%] Built target LoadEhrFX
$ strings build/LoadEhr.bundle/Contents/MacOS/LoadEhr | grep "LoadEhr by"   ->  LoadEhr by AmritusG
$ plutil -p build/LoadEhrWowFX.bundle/Contents/Info.plist | grep Identifier  ->  "CFBundleIdentifier" => "com.amritusg.loadehrwowfx"
$ for f in tools/*.py; do python3 -m py_compile $f; done     ->  39 compiled, 0 FAIL
$ tools/regress.sh golive
ref-hash guard ok: 6 references match tools/regress_refs.sha256
compare verify/w1/ref/r1_mask_front.png vs verify/golive/r1_mask_front.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r2_wireframe50.png vs verify/golive/r2_wireframe50.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r3_explode.png vs verify/golive/r3_explode.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r4_fx_skin_env_input.png vs verify/golive/r4_fx_skin_env_input.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r5_chain_mid_hop_b_c.png vs verify/golive/r5_chain_mid_hop_b_c.png: mean 0.0000 max 0 pixels>2 0 of 2073600
compare verify/w1/ref/r6_chain_rest_d.png vs verify/golive/r6_chain_rest_d.png: mean 0.0000 max 0 pixels>2 0 of 2073600
G1 ok / G4 ok / G6 ok   all four bundles (487 / 508 / 497 / 510 params; fresh instance covers 0 px, 4 empty slots)
G2 ok   the only directory API: ModelLoader.cpp FolderIndex
G3 ok   no model / texture file inside build/LoadEhr*.bundle (8 files: binaries + Info.plist)
G5 ok   no Laser* / Library* sources, no library tools
G6 ok   no Mirror* source, no MIRROR program variant
G7 ok   no libtess2 / tesselator / SGI-B in src, third_party, CMakeLists.txt
G7 ok   no LGPL file in the tree (1051 tracked files, submodules included)
$ python3 tools/param_audit.py static
static: 708 ids, carried {'LoadEhr': 487, 'LoadEhrFX': 508, 'LoadEhrWow': 497, 'LoadEhrWowFX': 510}; not read / misrouted: 0
$ python3 tools/param_audit.py canuse
canuse: 66 offered rows checked, 2 kept by Amrit, CANNOT USE left: 0
$ harness: text Model_A ~/Documents/Resolume Arena/MaskEhr Library/Skulls/Human Skull/scene.gltf
wait_loaded after 0.25 s: SWAP to ... /Skulls/Human Skull/scene.gltf (slot A, 31016 tris, 1 parts) 158 ms after request
$ gh repo edit AmritusG/loadehr --visibility public --accept-visibility-change-consequences
DENIED by the Claude Code auto mode classifier: [Create Public Surface]   (not run; repo still private)
$ git -C third_party/dxvkit grep -c <user name>
docs/handoffs/dxvkit_2026-10-08_w3-alley-fair-compare-r2.md:4
docs/handoffs/dxvkit_2026-10-08_w3-alley-fair-compare.md:4
docs/handoffs/dxvkit_2026-10-08_w3-encoder-results.md:13
```
Not yet available (owed after the flip): the two HTTP 200s and the credential-free recursive clone.
END OF HANDOFF go-public sha256:1eb1c3b4 10138
