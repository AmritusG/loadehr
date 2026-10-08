project:    LoadEhr
pass:       w3-dxv
STATUS:     BLOCKED
parent:     7f4bf14
result:     NONE
session:    44ca7128-e1da-4ce2-8f0a-77781723428a
written_at: 2026-10-08T14:28:00+02:00

# LoadEhr W3 (DXV via DXVKit + Video Alpha): BLOCKED on a permission

## What blocked
The Claude Code auto-mode classifier refused two actions as "Untrusted Code Integration":
1. `git submodule add https://github.com/AmritusG/dxvkit.git third_party/dxvkit` (+ checkout cdbb4bc).
2. Writing `src/Video.mm` code that includes `dxvkit.h` and calls `dxvk_mov_open_file` / `dxvk_decode_blocks`.
I did not try to get round it. Everything DXV (steps IMPLEMENT 1-3 and BUILD + TEST 1, 3, 5, 6, 7 of the brief) waits on it.
DXVKit itself was only read (README, dxvkit.h, dxvkit.hpp, docs/design.md); cdbb4bc is on the remote (HEAD 0e1a968 adds only a handoff).

## WHAT AMRIT DOES (one of the two)
- A) WHERE: this Claude Code session. HOW: run `/permissions`, add an Allow rule for Bash `git submodule add https://github.com/AmritusG/dxvkit.git third_party/dxvkit`, then tell me "go": I add the submodule and write the DXV player.
- B) WHERE: the prompt of this session. HOW: type `! git submodule add https://github.com/AmritusG/dxvkit.git third_party/dxvkit` (one line), then `! git -C third_party/dxvkit checkout cdbb4bc`, then tell me "go". If the classifier still refuses the code that calls DXVKit, only A) (or a looser auto mode) helps.

## Done and verified (uncommitted, in the working tree; parent 7f4bf14)
- **Video Alpha** (Projector group, Wow + Wow FX): OPT Premultiplied / Straight, default Premultiplied. Straight = colour x alpha in
  the copy shader (kProjVideoFS `uStraight`, W2's behaviour); Premultiplied = colour as is. HAP alpha-only (mode 4) is the alpha
  itself, unchanged either way. Changing the switch redraws the held frame (also on a paused / Once-held clip).
  Param counts (guards): **LoadEhr 482, FX 486, Wow 488, Wow FX 502** (<= 512).
- **W2 files, frame 0, harness 960x540 (Video Speed 0), vs the W2 build** (verify/w3/alpha/alpha_run.py):
  h264_1080, hevc_1080, prores422_1080, hap_1080, hap_4k, hapq_1080: max 0 in BOTH modes.
  prores4444a / hapalpha / hapqalpha: Straight max 0 (= W2); Premultiplied mean 2.55, max 148-149, 26.2k px differ.
  So the W2 alpha files change at the new default ON PURPOSE. Strip (Straight | Premultiplied): verify/w3/alpha/strip_prores4444a.png
  — our test file keeps colour where alpha is 0 (straight file), so read as Premultiplied the whole frame lights the skull.
- **G7 extended**: no LGPL file in the tree, submodules included (SPDX LGPL tag, GNU Lesser/Library GPL text, COPYING.LESSER/LIB).
  Clean tree: ok (290 files); a planted `SPDX-License-Identifier: LGPL-2.1-or-later` file: FAIL as it should.
- **GL check (M3 Max, "4.1 Metal - 90.5")**: EXT_texture_compression_s3tc present; GL_COMPRESSED_TEXTURE_FORMATS lists only
  0x83F0 / 0x83F2 / 0x83F3, but RGTC1 (0x8DBB, BC4) and RGTC2 (0x8DBD, BC5) both accept glCompressedTexImage2D + SubImage with no
  error (core since 3.0); BPTC 0x8E8C: GL_INVALID_ENUM. So every DXVKit plane format uploads as is. GL 4.1 has no glGetInternalformativ.
- regress: 6 references max 0; guards G1-G7 ok on all four bundles.

## Plan ready for "go" (not written)
VideoPlayer::Run tries `dxvk_mov_open_file` first (any failure -> AVFoundation as today); a DXV file reads + decodes the wanted
frame directly (random access) into BCn planes; the decode window / late / Loop / Ping-Pong / Once / Speed / Restart / clk.video stay
shared (Window() factored out of Run). Formats: DXT1 -> 0x83F0, DXT5 -> 0x83F3, YCG6 -> BC4 Y + RGTC2 CoCg (half size) on the
existing uDxt / uDxtA samplers (no new sampler), YG10 -> RGTC2 Y+A + RGTC2 CoCg; UV x display / coded crops the 16-px padding;
YCoCg = Hap Q's formula with scale 1 (README: R = Y - cg + co, G = Y + cg, B = Y - cg - co). Proof dump via a debug env var.

## Open
- Commit / push not done: the wave is not finished. Nothing in Arena touched (no lock taken).
END OF HANDOFF w3-dxv sha256:6c3aafc5 3817
