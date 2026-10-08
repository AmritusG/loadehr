#!/usr/bin/env python3
"""LoadEhr W9 Stage E: Shape Target (Spherize generalised: Sphere / Cube / Torus / Crystal; every bundle).

usage: tools/w9_shape.py renders <out>          each target on the skull, the SciFi helmet, text "LOADEHR" and our SVG emblem at Spherize 0.6
                                               and 1 ("too much": Spherize 1 x Deform Amount 2) -> renders.md
       tools/w9_shape.py strips <out>           Spherize 0 .. 1 .. 0 per target (forward = backward), + Twist / Shatter / the morph chain on
                                               top -> strips.md
       tools/w9_shape.py same <out> <ref build> Shape Target Sphere = the reference build's Spherize (0.3 / 0.7 / 1, skull + text): max diff
       tools/w9_shape.py cost <out>             harness 1080p GPU ms per target -> cost.md"""
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w5_wire as W  # noqa: E402

TARGETS = ['sphere', 'cube', 'torus', 'crystal']


def shot(out, tag, settle='frames 4'):
    return [settle, 'composite arena', 'shot %s/%s.png' % (out, tag)]


def renders(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| model | target | Spherize | shot |', '|---|---|---|---|']
    for name, m in W.MODELS:
        L = W.load(m, bundle='LoadEhr') + ['realtime 1']
        for t, tn in enumerate(TARGETS):
            for tag, cmd in (('06', ['set Spherize 0.6', 'set Deform Amount 1']), ('top', ['set Spherize 1', 'set Deform Amount 2'])):
                L += ['set Shape Target %d' % t] + cmd + shot(out, '%s_%s_%s' % (name, tn, tag))
                rows.append('| %s | %s | %s | %s_%s_%s.png |' % (name, tn, '0.6' if tag == '06' else '1 x Amount 2', name, tn, tag))
        W.harness(L, out, 'r_' + name)
    print('\n'.join(rows))
    open(os.path.join(out, 'renders.md'), 'w').write('\n'.join(rows) + '\n')


def strips(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| strip | forward = backward (max) |', '|---|---|']
    steps = [0, 0.25, 0.5, 0.75, 1.0]
    for t, tn in enumerate(TARGETS):
        L = W.load(W.SKULL, bundle='LoadEhr') + ['set Shape Target %d' % t]
        tags = []
        for i, a in enumerate(steps + steps[-2::-1]):
            tag = '%s_%02d' % (tn, i)
            L += ['set Spherize %g' % a] + shot(out, tag)
            tags.append(tag)
        k = len(steps)
        L += ['compare %s/%s.png %s/%s.png' % (out, tags[i], out, tags[2 * k - 2 - i]) for i in range(k - 1)]
        so, _ = W.harness(L, out, 's_' + tn)
        m = [int(x) for x in re.findall(r'max (\d+) pixels', so)]
        rows.append('| %s, Spherize 0 .. 1 .. 0 | %s |' % (tn, max(m) if m else '?'))
    for tag, cmd in (('torus_twist', ['set Shape Target 2', 'set Spherize 0.7', 'set Twist 2']),
                     ('cube_shatter', ['set Shape Target 1', 'set Spherize 0.8', 'realtime 0.5', 'pulse Shatter', 'realtime 0.6']),
                     ('crystal_midhop', ['mark', 'text Model_B ' + W.HELMET, 'wait_slots 1 60', 'set Shape Target 3', 'set Spherize 0.7', 'set Transition 0.5'])):
        W.harness(W.load(W.SKULL, bundle='LoadEhr') + cmd + shot(out, tag, 'realtime 1'), out, 'c_' + tag)
        rows.append('| combination %s | %s.png |' % (tag, tag))
    print('\n'.join(rows))
    open(os.path.join(out, 'strips.md'), 'w').write('\n'.join(rows) + '\n')


def same(out, ref):
    os.makedirs(out, exist_ok=True)
    rows = ['| model | Spherize | max diff vs %s |' % os.path.basename(ref.rstrip('/')), '|---|---|---|']
    for name, m in (('skull', W.SKULL), ('text', 'text')):
        for build, side in ((os.path.join(W.ROOT, 'build'), 'new'), (ref, 'ref')):
            L = W.load(m, bundle='LoadEhr')
            L[0] = 'bundle %s/LoadEhr.bundle' % build
            L += ['realtime 1']
            for a in (0.3, 0.7, 1.0):
                L += ['set Spherize %g' % a, 'advance 0 zero', 'hosttime 5', 'frames 3', 'shot %s/%s_%g_%s.png' % (out, name, a, side)]
            sp = os.path.join(out, '%s_%s.txt' % (name, side))
            open(sp, 'w').write('\n'.join(L) + '\n')
            import subprocess
            subprocess.run([os.path.join(build, 'loadehr_harness'), sp], capture_output=True)
        L = ['compare %s/%s_%g_ref.png %s/%s_%g_new.png' % (out, name, a, out, name, a) for a in (0.3, 0.7, 1.0)]
        so, _ = W.harness(L, out, 'cmp_' + name)
        for a, mx in zip((0.3, 0.7, 1.0), re.findall(r'max (\d+) pixels', so)):
            rows.append('| %s | %g | %s |' % (name, a, mx))
    print('\n'.join(rows))
    open(os.path.join(out, 'same.md'), 'w').write('\n'.join(rows) + '\n')


def cost(out):
    os.makedirs(out, exist_ok=True)
    cases = [('solid', ['set Spherize 0'])] + [(tn, ['set Shape Target %d' % t, 'set Spherize 0.7']) for t, tn in enumerate(TARGETS)]
    rows = ['| case | skull plugin ms avg / GPU ms avg | helmet |', '|---|---|---|']
    res = {}
    for name, m in W.MODELS[:2]:
        L = W.load(m, (1920, 1080), bundle='LoadEhr') + ['set Spin Y 20', 'realtime 1.5']
        for i, (c, w) in enumerate(cases):
            L += w + ['realtime 1', 'stats_reset', 'frames 120', 'stats C%d' % i]
        so, _ = W.harness(L, out, 'cost_' + name)
        for i in range(len(cases)):
            m2 = re.search(r'stats C%d\s.*?plugin wall ms avg\s+([\d.]+).*?GPU ms avg\s+([\d.]+)' % i, so)
            res[(name, i)] = '%s / %s' % m2.groups() if m2 else '?'
    for i, (c, _) in enumerate(cases):
        rows.append('| %s | %s | %s |' % (c, res[('skull', i)], res[('helmet', i)]))
    print('\n'.join(rows))
    open(os.path.join(out, 'cost.md'), 'w').write('\n'.join(rows) + '\n')


if __name__ == '__main__':
    a = sys.argv
    {'renders': lambda: renders(a[2]), 'strips': lambda: strips(a[2]), 'same': lambda: same(a[2], a[3]), 'cost': lambda: cost(a[2])}[a[1]]()
