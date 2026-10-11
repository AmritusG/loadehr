#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""LoadEhr W13: C Auto Place Once Per Model follows each model's depth / cone (Amrit on W12's C2: "just looks off, incorrect, flawed").

usage: tools/w13_once.py hop <out> <ref build> [damaged|goat]  beam only, slider 0 -> 1 -> 0 (tools/w12_glide.py's hop): Once Per Model and
                                                              Every Model Swap, this build and the reference build (W12); dials read back
       tools/w13_once.py arrive <out> <ref build>             resting on model B after a hop, Once Per Model vs Every Model Swap (the same
                                                              openings: now the same depth / cone), damaged + goat, with haze (beams) and
                                                              without (bit-exact); + Every vs Every repeated (the run-to-run floor)
       tools/w13_once.py same <out> <ref build>               Off / Position Only / Every Model Swap: this build vs the reference build (no haze)
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w12_glide as G  # noqa: E402

MODES = {'Once': 1, 'Every': 3}


def compare(out, a, b):
    m = re.search(r'mean [\d.]+ max \d+ pixels>2 \d+', G.run(G.BUILD, ['compare %s/%s.png %s/%s.png' % (out, a, out, b)], out, 'cmp'))
    return m.group(0) if m else '?'


def hop(out, ref, second='damaged'):
    os.makedirs(out, exist_ok=True)
    N, F = 100, 8
    rows = ['| build | C Auto Place | direction | max step (at t) | step at the swap | mean step t 0.35-0.65 | mean step elsewhere |', '|---|---|---|---|---|---|---|']
    reads = ['| build | C Auto Place | at | ' + ' | '.join(G.CDIALS) + ' |', '|---|---|---|' + '---|' * len(G.CDIALS)]
    for build, bn in ((G.BUILD, 'W13'), (ref, 'W12')):
        for mn, mi in MODES.items():
            L = G.scene(build, mi, G.SECOND[second]) + ['set Transition 0', 'frames %d' % F, 'ref_save prev'] + ['get %s' % p for p in G.CDIALS]
            for d, seq in (('fwd', range(1, N + 1)), ('back', range(N - 1, -1, -1))):
                for i in seq:
                    L += ['set Transition %g' % (i / N), 'frames %d' % F, 'ref_cmp %s:%d prev' % (d, i), 'ref_save prev']
                L += ['realtime 1'] + ['get %s' % p for p in G.CDIALS]
            txt = G.run(build, L, out, 'hop_%s_%s_%s' % (second, bn, mn))
            v = G.gets(txt)
            for k, at in enumerate(('rest on skull (t 0)', 'arrived on B (t 1)', 'back on skull (t 0)')):
                reads.append('| %s | %s | %s | %s |' % (bn, mn, at, ' | '.join(v.get(p, ['?'] * 3)[k] if len(v.get(p, [])) > k else '?' for p in G.CDIALS)))
            for d in ('fwd', 'back'):
                s = G.steps(txt, d)
                if not s:
                    rows.append('| %s | %s | %s | harness failed | | | |' % (bn, mn, d))
                    continue
                mx = max(s, key=lambda x: x[1])
                swap = max(c for i, c in s if (50 <= i <= 51 if d == 'fwd' else 49 <= i <= 50))
                win = [c for i, c in s if 35 <= i <= 65]
                rest = [c for i, c in s if not 35 <= i <= 65]
                rows.append('| %s | %s | %s | %.3f (%.2f) | %.3f | %.3f | %.3f |' % (bn, mn, d, mx[1], mx[0] / N, swap, sum(win) / len(win), sum(rest) / len(rest)))
    txt = '\n'.join(rows) + '\n\n' + '\n'.join(reads) + '\n'
    print(txt)
    open(os.path.join(out, 'hop_%s.md' % second), 'w').write(txt)


def arrive(out, ref):
    os.makedirs(out, exist_ok=True)
    rows = ['| B | haze | Once vs Every, W13 | Once vs Every, W12 | Every vs Every (repeat, W13) |', '|---|---|---|---|---|']
    for second in ('damaged', 'goat'):
        for haze in (0.25, 0):
            tag = '%s_h%g' % (second, haze)
            # the first run of a series is a cold-start outlier with haze (W12): one thrown away
            for build, bn, mn in ((G.BUILD, 'warm', 'Every'), (G.BUILD, 'W13', 'Once'), (G.BUILD, 'W13', 'Every'), (G.BUILD, 'W13b', 'Every'),
                                  (ref, 'W12', 'Once'), (ref, 'W12', 'Every')):
                L = G.scene(build, MODES[mn], G.SECOND[second])[:-1] + ['set %d 1' % (156 + 4 * k) for k in range(8)] + ['set Haze %g' % haze]
                L += ['set Transition 0', 'frames 30', 'set Transition 0.5', 'frames 30', 'set Transition 1', 'realtime 3',
                      'hosttime 100', 'advance 0 zero', 'frames 150', 'shot_rgba %s/%s_%s_%s.png' % (out, tag, bn, mn)]
                G.run(build, L, out, '%s_%s_%s' % (tag, bn, mn))
            rows.append('| %s | %g | %s | %s | %s |' % (second, haze, compare(out, tag + '_W13_Once', tag + '_W13_Every'),
                                                        compare(out, tag + '_W12_Once', tag + '_W12_Every'), compare(out, tag + '_W13_Every', tag + '_W13b_Every')))
    txt = '\n'.join(rows) + '\n'
    print(txt)
    open(os.path.join(out, 'arrive.md'), 'w').write(txt)


def same(out, ref):
    os.makedirs(out, exist_ok=True)
    ts = (0, 0.3, 0.5, 0.7, 1)
    rows = ['| C Auto Place | t | W13 vs W12 | W13 vs W13 (repeat) |', '|---|---|---|---|']
    for mi, mn in ((0, 'Off'), (2, 'Position'), (3, 'Every')):
        for build, bn in ((G.BUILD, 'W13'), (G.BUILD, 'W13b'), (ref, 'W12')):
            L = G.scene(build, mi, G.GOAT)[:-1] + ['set %d 1' % (156 + 4 * k) for k in range(8)] + ['set Haze 0', 'hosttime 100', 'advance 0 zero']
            for t in ts:
                L += ['set Transition %g' % t, 'frames 20', 'shot_rgba %s/same_%s_%s_%g.png' % (out, mn, bn, t)]
            G.run(build, L, out, 'same_%s_%s' % (mn, bn))
        for t in ts:
            rows.append('| %s | %g | %s | %s |' % (mn, t, compare(out, 'same_%s_W13_%g' % (mn, t), 'same_%s_W12_%g' % (mn, t)),
                                                   compare(out, 'same_%s_W13_%g' % (mn, t), 'same_%s_W13b_%g' % (mn, t))))
    txt = '\n'.join(rows) + '\n'
    print(txt)
    open(os.path.join(out, 'same.md'), 'w').write(txt)


if __name__ == '__main__':
    a = sys.argv[1:]
    if len(a) in (3, 4) and a[0] == 'hop':
        hop(a[1], a[2], *a[3:])
    elif len(a) == 3 and a[0] in ('arrive', 'same'):
        {'arrive': arrive, 'same': same}[a[0]](a[1], a[2])
    else:
        print(__doc__)
        sys.exit(1)
