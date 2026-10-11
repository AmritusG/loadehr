#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""LoadEhr W12: Light C's cone / Behind Depth glide with C Auto Place Every Model Swap, proven in the harness.

usage: tools/w12_glide.py hop <out> <ref build> [damaged|goat]  beam only (every part hidden, haze, no smoke), skull (A) -> Damaged Helmet
                                                      (B; goat: Goat Skull, whose fit changes the cone too): the
                                                      Transition slider walked 0 -> 1 -> 0 in 100 steps each way (8 frames a step), the mean
                                                      change per step, Every Model Swap and Once Per Model, this build and the reference
                                                      build (W11); + the C dials read back at each end
       tools/w12_glide.py interrupt <out> <ref build>  skull -> Goat Skull -> Damaged Helmet: resting mid-hop A -> B (Transition 0.35), the slider
                                                      jumps into hop B -> C (0.8); the change per frame for 2.5 s after the jump
       tools/w12_glide.py same <out> <ref build>       C Auto Place Off / Once Per Model / Position Only, models shown, lit by Light C (no haze), skull -> Goat Skull
                                                      at Transition 0 / 0.3 / 0.5 / 0.7 / 1: this build vs the reference build, and this build twice
                                                      (the run-to-run floor)
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w5_wire as W  # noqa: E402

ROOT = W.ROOT
BUILD = os.path.join(ROOT, 'build')
DAMAGED = os.path.join(ROOT, 'test_models', 'DamagedHelmet', 'DamagedHelmet.glb')
GOAT = os.path.join(W.LIB, 'Skulls', 'Goat Skull', 'scene.gltf')  # see-through eyes: from the skull's Behind Depth a 24 deg cone (the others cap at 40)
SECOND = {'damaged': DAMAGED, 'goat': GOAT}
CMP = re.compile(r'^ref_cmp (\S+) mean ([\d.]+) max (\d+)')
PRESET = [('C Aim Mode', 1), ('C Cone Angle', 70), ('C Behind Depth', 1.5), ('C Anchor To Mask', 0)]  # W11's preset: off-aim
CDIALS = ['C Cone Angle', 'C Behind Depth', 'C Left Pos X', 'C Left Pos Y', 'C Left Pos Z']
MODES = {'Every': 3, 'Once': 1}


def run(build, lines, out, tag):
    sp = os.path.join(out, tag + '.txt')
    open(sp, 'w').write('\n'.join(lines) + '\n')
    r = subprocess.run([os.path.join(build, 'loadehr_harness'), sp], capture_output=True, text=True, cwd=ROOT)
    open(os.path.join(out, tag + '.out'), 'w').write(r.stdout + r.stderr)
    return r.stdout + r.stderr


def scene(build, mode, b=DAMAGED, c=None):
    L = ['bundle %s/LoadEhr.bundle' % build, 'size 480 270', 'instance', 'text Model_A ' + W.SKULL, 'wait_loaded 60',
         'mark', 'text Model_B ' + b, 'wait_slots 1 60']
    if c:
        L += ['mark', 'text Model_C ' + c, 'wait_slots 1 60']
    L += ['set %d 0' % (156 + 4 * k) for k in range(8)]  # Part 1..8 Visible off (LoadEhr indices): the beams only
    L += ['set Smoke Amount 0', 'set Haze 0.25', 'set Orbit Yaw 60', 'set C Auto Place %d' % mode]
    L += ['set %s %g' % kv for kv in PRESET] + ['set C Enable 1', 'realtime 3', 'hosttime 100', 'advance 0 zero']
    return L


def gets(text):
    v = {}
    for l in text.splitlines():
        m = re.match(r'^get (.+?) = (\S+)', l)
        if m:
            v.setdefault(m.group(1), []).append(m.group(2))
    return v


def steps(text, prefix):
    return [(int(m.group(1).split(':')[1]), float(m.group(2))) for m in map(CMP.match, text.splitlines()) if m and m.group(1).startswith(prefix + ':')]


def hop(out, ref, second='damaged'):
    os.makedirs(out, exist_ok=True)
    N, F = 100, 8
    rows = ['| build | C Auto Place | direction | max step (at t) | step at the swap (t 0.50-0.51) | mean step t 0.35-0.65 | mean step elsewhere |',
            '|---|---|---|---|---|---|---|']
    reads = ['| build | C Auto Place | at | ' + ' | '.join(CDIALS) + ' |', '|---|---|---|' + '---|' * len(CDIALS)]
    for build, bn in ((BUILD, 'W12'), (ref, 'W11')):
        for mn, mi in MODES.items():
            L = scene(build, mi, SECOND[second]) + ['set Transition 0', 'frames %d' % F, 'ref_save prev'] + ['get %s' % p for p in CDIALS]
            for d, seq in (('fwd', range(1, N + 1)), ('back', range(N - 1, -1, -1))):
                for i in seq:
                    L += ['set Transition %g' % (i / N), 'frames %d' % F, 'ref_cmp %s:%d prev' % (d, i), 'ref_save prev']
                L += ['realtime 1'] + ['get %s' % p for p in CDIALS]
            txt = run(build, L, out, 'hop_%s_%s_%s' % (second, bn, mn))
            v = gets(txt)
            for k, at in enumerate(('rest on skull (t 0)', 'arrived on B (t 1)', 'back on skull (t 0)')):
                reads.append('| %s | %s | %s | %s |' % (bn, mn, at, ' | '.join(v.get(p, ['?'] * 3)[k] if len(v.get(p, [])) > k else '?' for p in CDIALS)))
            for d in ('fwd', 'back'):
                s = steps(txt, d)
                if not s:
                    rows.append('| %s | %s | %s | harness failed | | | |' % (bn, mn, d))
                    continue
                # step i = the change from the previous slider value to i / N (fwd: from (i-1)/N, back: from (i+1)/N)
                mx = max(s, key=lambda x: x[1])
                swap = max(c for i, c in s if (50 <= i <= 51 if d == 'fwd' else 49 <= i <= 50))
                win = [c for i, c in s if 35 <= i <= 65]
                rest = [c for i, c in s if not 35 <= i <= 65]
                rows.append('| %s | %s | %s | %.3f (%.2f) | %.3f | %.3f | %.3f |' % (bn, mn, d, mx[1], mx[0] / N, swap, sum(win) / len(win), sum(rest) / len(rest)))
                open(os.path.join(out, 'hop_%s_%s_%s_%s.csv' % (second, bn, mn, d)), 'w').write('\n'.join('%g,%g' % (i / N, c) for i, c in s) + '\n')
    txt = '\n'.join(rows) + '\n\n' + '\n'.join(reads) + '\n'
    print(txt)
    open(os.path.join(out, 'hop_%s.md' % second), 'w').write(txt)


def interrupt(out, ref):
    os.makedirs(out, exist_ok=True)
    rows = ['| build | C Auto Place | frame 1 after the jump | max per frame (frame) | sum frames 1-150 | frames > 0.5 |', '|---|---|---|---|---|---|']
    for build, bn in ((BUILD, 'W12'), (ref, 'W11')):
        for mn, mi in MODES.items():
            L = scene(build, mi, GOAT, DAMAGED) + ['set Transition 0.35', 'realtime 2', 'frames 60', 'ref_save prev']
            for f in range(1, 151):
                L += (['set Transition 0.8'] if f == 1 else []) + ['frames 1', 'ref_cmp jump:%d prev' % f, 'ref_save prev']
            L += ['get %s' % p for p in CDIALS]
            txt = run(build, L, out, 'interrupt_%s_%s' % (bn, mn))
            s = steps(txt, 'jump')
            if not s:
                rows.append('| %s | %s | harness failed | | | |' % (bn, mn))
                continue
            mx = max(s, key=lambda x: x[1])
            rows.append('| %s | %s | %.3f | %.3f (%d) | %.2f | %d |' % (bn, mn, s[0][1], mx[1], mx[0], sum(c for _, c in s), sum(1 for _, c in s if c > 0.5)))
            open(os.path.join(out, 'interrupt_%s_%s.csv' % (bn, mn)), 'w').write('\n'.join('%d,%g' % x for x in s) + '\n')
    txt = '\n'.join(rows) + '\n'
    print(txt)
    open(os.path.join(out, 'interrupt.md'), 'w').write(txt)


def same(out, ref):
    os.makedirs(out, exist_ok=True)
    ts = (0, 0.3, 0.5, 0.7, 1)
    for mi, mn in ((0, 'Off'), (1, 'Once'), (2, 'Position')):
        # no haze: the volume march is not run-to-run exact (multimodal, measured W12: two runs of one build differ up to max 66)
        for build, bn in ((BUILD, 'W12'), (BUILD, 'W12b'), (ref, 'W11')):
            L = scene(build, mi, GOAT)[:-1] + ['set %d 1' % (156 + 4 * k) for k in range(8)] + ['set Haze 0', 'hosttime 100', 'advance 0 zero']
            for t in ts:
                L += ['set Transition %g' % t, 'frames 20', 'shot_rgba %s/same_%s_%s_%g.png' % (out, mn, bn, t)]
            run(build, L, out, 'same_%s_%s' % (mn, bn))
    def cmp(a, b):
        m = re.search(r'mean [\d.]+ max \d+ pixels>2 \d+', run(BUILD, ['compare %s/same_%s.png %s/same_%s.png' % (out, a, out, b)], out, 'cmp'))
        return m.group(0) if m else '?'
    rows = ['| C Auto Place | t | W12 vs W11 | W12 vs W12 (repeat) | what the mode changes (vs Off, W12) |', '|---|---|---|---|---|']
    for mn in ('Off', 'Once', 'Position'):
        for t in ts:
            rows.append('| %s | %g | %s | %s | %s |' % (mn, t, cmp('%s_W12_%g' % (mn, t), '%s_W11_%g' % (mn, t)), cmp('%s_W12_%g' % (mn, t), '%s_W12b_%g' % (mn, t)),
                                                       '-' if mn == 'Off' else cmp('%s_W12_%g' % (mn, t), 'Off_W12_%g' % t)))
    txt = '\n'.join(rows) + '\n'
    print(txt)
    open(os.path.join(out, 'same.md'), 'w').write(txt)


if __name__ == '__main__':
    a = sys.argv[1:]
    if len(a) in (3, 4) and a[0] == 'hop':
        hop(a[1], a[2], *a[3:])
    elif len(a) == 3 and a[0] == 'same':
        same(a[1], a[2])
    elif len(a) == 3 and a[0] == 'interrupt':
        interrupt(a[1], a[2])
    else:
        print(__doc__)
        sys.exit(1)
