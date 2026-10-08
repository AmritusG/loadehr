#!/usr/bin/env python3
"""LoadEhr W9 Stage C: the Cut looks (Cross-Section, Reveal Box, Window; every bundle) in the harness.

usage: tools/w9_cut.py renders <out>   every Cut Look on the skull (slot B = the SciFi helmet), the helmet (B = skull), text "LOADEHR" and our
                                      SVG emblem (B = skull), default + the "too much" end -> renders.md (Wow bundle; the emblem in LoadEhr)
       tools/w9_cut.py strips <out>    Cut Position 0 .. 1 .. 0 (Cross-Section: forward = backward), Cut Speed sweeps / drifts / turns,
                                      Reveal Box roundness 0 .. 1 -> strips.md
       tools/w9_cut.py combos <out>    Cross-Section + sim smoke (Emit Inside), Window + the projector's video (DXV), Reveal Box + Points
                                      Only, Cross-Section + Print On's classic bundle (LoadEhr), Window mid-hop -> combos.md
       tools/w9_cut.py cost <out>      harness 1080p plugin / GPU ms per look (default / top) -> cost.md"""
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import w5_wire as W  # noqa: E402

LOOKS = [(1, 'cross'), (2, 'reveal'), (3, 'window')]
DXV = os.path.join(W.ROOT, 'test_media', 'gen', 'w4', 'code_dxv_1080.mov')


def cut(n, extra=()):
    return ['set Cut Look %d' % n] + ['set %s %g' % kv for kv in extra]


TOP = {1: (('Cut Size', 0.05), ('Cut Position', 0.5)), 2: (('Cut Size', 1), ('Cut Position', 1), ('Cut Speed', 30)), 3: (('Cut Size', 1), ('Cut Position', 1), ('Cut Speed', 30))}


def shot(out, tag, settle='realtime 1.2'):
    return [settle, 'composite arena', 'shot %s/%s.png' % (out, tag), 'shot_rgba %s/%s_raw.png' % (out, tag)]


def cov(out, tag):
    c = W.coverage('%s/%s_raw.png' % (out, tag))
    os.remove('%s/%s_raw.png' % (out, tag))
    return c


def with_b(model, b, bundle='LoadEhrWow'):
    return W.load(model, bundle=bundle) + ['mark', 'text Model_B ' + b, 'wait_slots 1 60']


def renders(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| model | look | case | coverage | shot |', '|---|---|---|---|---|']
    for name, m, b, bun in (('skull', W.SKULL, W.HELMET, 'LoadEhrWow'), ('helmet', W.HELMET, W.SKULL, 'LoadEhrWow'), ('text', 'text', W.SKULL, 'LoadEhrWow'),
                            ('logo', 'logo', W.SKULL, 'LoadEhr')):
        L = with_b(m, b, bun) + ['set Rotate Y 30', 'set Orbit Pitch 15', 'realtime 1']
        tags = []
        for n, ln in LOOKS:
            for case, extra in (('default', ()), ('too much', TOP[n])):
                tag = '%s_%s_%s' % (name, ln, 'def' if not extra else 'top')
                L += ['set Cut Size 0.4', 'set Cut Position 0.5', 'set Cut Speed 0'] + cut(n, extra) + shot(out, tag, 'realtime 2')
                tags.append((ln, case, tag))
        W.harness(L, out, 'r_' + name)
        for ln, case, tag in tags:
            rows.append('| %s | %s | %s | %.4f | %s.png |' % (name, ln, case, cov(out, tag), tag))
        print('\n'.join(rows[-len(tags):]), flush=True)
    open(os.path.join(out, 'renders.md'), 'w').write('\n'.join(rows) + '\n')


def strips(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| strip | coverage per step | forward = backward |', '|---|---|---|']
    steps = [0.1, 0.3, 0.5, 0.7, 0.9]
    L = W.load(W.SKULL) + ['set Rotate Y 50', 'set Build Axis 2', 'set Cut Size 1'] + cut(1)
    tags = []
    for i, a in enumerate(steps + steps[-2::-1]):
        tag = 'cross_%02d' % i
        L += ['set Cut Position %g' % a] + shot(out, tag, 'frames 4')
        tags.append(tag)
    k = len(steps)
    L += ['compare %s/%s.png %s/%s.png' % (out, tags[i], out, tags[2 * k - 2 - i]) for i in range(k - 1)]
    so, _ = W.harness(L, out, 's_cross')
    m = [int(x) for x in re.findall(r'max (\d+) pixels', so)]
    rows.append('| cross-section, Cut Position 0.1 .. 0.9 .. 0.1 (Build Axis Z) | %s | %s |' % (' '.join('%.3f' % cov(out, t) for t in tags), max(m) if m else '?'))
    L = W.load(W.SKULL) + cut(2, (('Cut Size', 0.45),))
    tags = []
    for i, r in enumerate((0, 0.5, 1)):
        tag = 'reveal_round_%d' % i
        L += ['set Cut Position %g' % r] + shot(out, tag, 'frames 4')
        tags.append(tag)
    W.harness(L, out, 's_reveal')
    rows.append('| reveal box, roundness 0 / 0.5 / 1 | %s | |' % ' '.join('%s %.3f' % (t, cov(out, t)) for t in tags))
    res = []
    for tag, cmd in (('cross_speed0', cut(1)), ('cross_speed20', cut(1, (('Cut Speed', 20),))), ('reveal_speed20', cut(2, (('Cut Speed', 20),))),
                     ('window_speed20', cut(3, (('Cut Speed', 20),)))):
        so, _ = W.harness(with_b(W.SKULL, W.HELMET) + cmd + ['realtime 1.5', 'diffs 60'], out, 'd_' + tag)
        d = [list(map(float, l.split()[1:])) for l in so.splitlines() if l.startswith('diffs:')]
        res.append('%s %.4f' % (tag, sum(d[0]) / len(d[0])) if d else tag + ' ?')
    rows.append('| mean per-frame change, model still | %s | |' % ', '.join(res))
    print('\n'.join(rows))
    open(os.path.join(out, 'strips.md'), 'w').write('\n'.join(rows) + '\n')


def combos(out):
    os.makedirs(out, exist_ok=True)
    rows = ['| combination | coverage | shot |', '|---|---|---|']
    smoke = [('Smoke Engine', 1), ('Emit Inside', 4), ('Smoke Ambient', 1.5), ('Smoke Amount', 2), ('Haze', 0)]
    for tag, L in (('cross_smoke', W.load(W.SKULL) + ['set Rotate Y 50', 'set Build Axis 2', 'set Cut Size 1'] + cut(1) + ['set %s %g' % kv for kv in smoke]),
                   ('window_video', W.load(W.SKULL) + ['set Rotate Y 10', 'text Proj_File ' + DXV, 'set Proj Enable 1', 'set Proj Beam 0', 'set Proj Surface 0.3'] + cut(3)),
                   ('reveal_points', W.load(W.HELMET) + ['set Wire Mode 4', 'set Point Source 2', 'set Wire Color Mix 1'] + cut(2, (('Cut Size', 0.5),))),
                   ('cross_classic_print', W.load(W.HELMET, bundle='LoadEhr') + ['set Orbit Pitch 25', 'set Build Look 2'] + cut(1, (('Cut Position', 0.6),))),
                   ('window_midhop', with_b(W.SKULL, W.HELMET) + ['mark', 'text Model_C ' + W.SKULL, 'wait_slots 1 60', 'set Transition 0.25'] + cut(3))):
        W.harness(L + shot(out, tag, 'realtime 3'), out, 'c_' + tag)
        rows.append('| %s | %.4f | %s.png |' % (tag, cov(out, tag), tag))
        print(rows[-1], flush=True)
    open(os.path.join(out, 'combos.md'), 'w').write('\n'.join(rows) + '\n')


def cost(out):
    os.makedirs(out, exist_ok=True)
    cases = [('solid (Cut Look Off)', ['set Cut Look 0'])]
    for n, ln in LOOKS:
        cases += [('%s default' % ln, ['set Cut Size 0.4', 'set Cut Position 0.5', 'set Cut Speed 0'] + cut(n)), ('%s top' % ln, cut(n, TOP[n]))]
    rows = ['| case | skull plugin ms avg / GPU ms avg | helmet |', '|---|---|---|']
    res = {}
    for name, m, b in (('skull', W.SKULL, W.HELMET), ('helmet', W.HELMET, W.SKULL)):
        L = with_b(m, b) + ['set Spin Y 20', 'realtime 1.5']
        L[1] = 'size 1920 1080'
        for i, (c, w) in enumerate(cases):
            L += w + ['realtime 1.5', 'stats_reset', 'frames 120', 'stats C%d' % i]
        so, _ = W.harness(L, out, 'cost_' + name)
        for i in range(len(cases)):
            m2 = re.search(r'stats C%d\s.*?plugin wall ms avg\s+([\d.]+).*?GPU ms avg\s+([\d.]+)' % i, so)
            res[(name, i)] = '%s / %s' % m2.groups() if m2 else '?'
    for i, (c, _) in enumerate(cases):
        rows.append('| %s | %s | %s |' % (c, res[('skull', i)], res[('helmet', i)]))
    print('\n'.join(rows))
    open(os.path.join(out, 'cost.md'), 'w').write('\n'.join(rows) + '\n')


if __name__ == '__main__':
    {'renders': renders, 'strips': strips, 'combos': combos, 'cost': cost}[sys.argv[1]](sys.argv[2])
